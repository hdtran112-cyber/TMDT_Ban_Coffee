from django.conf import settings
from django.db import models

from catalog.models import Product


class Cart(models.Model):
    """Mỗi người dùng có đúng một giỏ hàng."""
    user = models.OneToOneField(
        settings.AUTH_USER_MODEL, on_delete=models.CASCADE, related_name="cart"
    )

    def __str__(self):
        return f"Giỏ hàng của {self.user}"


class CartItem(models.Model):
    """Một dòng trong giỏ: một sản phẩm kèm số lượng."""
    cart = models.ForeignKey(Cart, on_delete=models.CASCADE, related_name="items")
    product = models.ForeignKey(Product, on_delete=models.CASCADE)
    quantity = models.PositiveIntegerField(default=1)

    class Meta:
        ordering = ["id"]
        constraints = [
            # Một sản phẩm chỉ xuất hiện một dòng trong giỏ, thêm lần nữa thì cộng dồn
            models.UniqueConstraint(fields=["cart", "product"], name="uniq_product_per_cart"),
        ]

    def __str__(self):
        return f"{self.product} x {self.quantity}"


class Order(models.Model):
    STATUS_CHOICES = [
        ("pending", "Chờ xác nhận"),
        ("confirmed", "Đã xác nhận"),
        ("shipping", "Đang giao"),
        ("delivered", "Đã giao"),
        ("cancelled", "Đã hủy"),
    ]
    PAYMENT_METHOD_CHOICES = [
        ("cod", "Thanh toán khi nhận hàng"),
        ("qr_mock", "Quét mã QR (giả lập)"),
    ]

    # PROTECT: không cho xóa người dùng đang có đơn hàng
    user = models.ForeignKey(
        settings.AUTH_USER_MODEL, on_delete=models.PROTECT, related_name="orders"
    )
    status = models.CharField(max_length=10, choices=STATUS_CHOICES, default="pending")
    total_vnd = models.PositiveIntegerField()
    shipping_name = models.CharField(max_length=100)
    phone = models.CharField(max_length=20)
    address = models.CharField(max_length=255)
    payment_method = models.CharField(max_length=10, choices=PAYMENT_METHOD_CHOICES)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["-created_at", "-id"]

    def __str__(self):
        return f"Đơn #{self.pk}"


class OrderItem(models.Model):
    order = models.ForeignKey(Order, on_delete=models.CASCADE, related_name="items")
    # PROTECT: sản phẩm đã được mua thì không xóa hẳn (chỉ ẩn bằng is_active)
    product = models.ForeignKey(Product, on_delete=models.PROTECT)
    quantity = models.PositiveIntegerField()
    # Giá tại lúc mua: giá sản phẩm đổi sau này thì đơn cũ không bị đổi theo
    unit_price_vnd = models.PositiveIntegerField()

    class Meta:
        ordering = ["id"]

    def __str__(self):
        return f"{self.product} x {self.quantity}"


class Payment(models.Model):
    STATUS_CHOICES = [
        ("unpaid", "Chưa thanh toán"),
        ("paid", "Đã thanh toán"),
        ("failed", "Thanh toán lỗi"),
    ]

    order = models.OneToOneField(Order, on_delete=models.CASCADE, related_name="payment")
    method = models.CharField(max_length=10, choices=Order.PAYMENT_METHOD_CHOICES)
    status = models.CharField(max_length=10, choices=STATUS_CHOICES, default="unpaid")
    transaction_code = models.CharField(max_length=30, blank=True)
    amount_vnd = models.PositiveIntegerField()
    paid_at = models.DateTimeField(null=True, blank=True)

    def __str__(self):
        return f"Thanh toán đơn #{self.order_id}: {self.status}"


class OrderStatusLog(models.Model):
    """Mỗi lần đơn đổi trạng thái ghi một dòng, để khách xem lịch sử."""
    order = models.ForeignKey(Order, on_delete=models.CASCADE, related_name="status_logs")
    status = models.CharField(max_length=10, choices=Order.STATUS_CHOICES)
    at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ["at", "id"]

    def __str__(self):
        return f"Đơn #{self.order_id} -> {self.status}"
