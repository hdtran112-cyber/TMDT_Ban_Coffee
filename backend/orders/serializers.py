from rest_framework import serializers

from catalog.models import Product
from .models import CartItem, Order, OrderItem, OrderStatusLog, Payment

QUANTITY_ERRORS = {
    "invalid": "Số lượng phải là số nguyên.",
    "min_value": "Số lượng phải từ 1 trở lên.",
}


# ---------- Giỏ hàng ----------

class AddToCartSerializer(serializers.Serializer):
    """Dữ liệu client gửi khi thêm vào giỏ."""
    product_id = serializers.IntegerField(error_messages={"invalid": "Mã sản phẩm không hợp lệ."})
    quantity = serializers.IntegerField(min_value=1, default=1, error_messages=QUANTITY_ERRORS)


class UpdateQuantitySerializer(serializers.Serializer):
    """Dữ liệu client gửi khi đổi số lượng một dòng."""
    quantity = serializers.IntegerField(min_value=1, error_messages=QUANTITY_ERRORS)


class CartProductSerializer(serializers.ModelSerializer):
    class Meta:
        model = Product
        fields = ["id", "name", "price_vnd", "image_url", "weight_g", "roast_level", "stock"]


class CartItemSerializer(serializers.ModelSerializer):
    product = CartProductSerializer(read_only=True)
    line_total_vnd = serializers.SerializerMethodField()
    available = serializers.SerializerMethodField()

    class Meta:
        model = CartItem
        fields = ["id", "product", "quantity", "line_total_vnd", "available"]

    def get_line_total_vnd(self, item):
        return item.product.price_vnd * item.quantity

    def get_available(self, item):
        # False nếu sản phẩm đã bị ẩn hoặc kho không còn đủ số lượng trong giỏ
        return item.product.is_active and item.product.stock >= item.quantity


def cart_payload(cart):
    """Giỏ hàng dạng JSON: danh sách dòng và tổng tiền (backend tự tính)."""
    items = CartItemSerializer(cart.items.select_related("product"), many=True).data
    total = sum(item["line_total_vnd"] for item in items)
    return {"items": items, "total_vnd": total}


# ---------- Đơn hàng ----------

class CheckoutSerializer(serializers.Serializer):
    """Thông tin giao hàng client gửi khi đặt hàng. Không có giá hay tổng tiền."""
    shipping_name = serializers.CharField(
        max_length=100,
        error_messages={
            "required": "Vui lòng nhập họ tên người nhận.",
            "blank": "Vui lòng nhập họ tên người nhận.",
            "max_length": "Họ tên tối đa 100 ký tự.",
        },
    )
    phone = serializers.RegexField(
        r"^(0|\+84)\d{9,10}$",
        error_messages={
            "required": "Vui lòng nhập số điện thoại.",
            "blank": "Vui lòng nhập số điện thoại.",
            "invalid": "Số điện thoại không hợp lệ.",
        },
    )
    address = serializers.CharField(
        max_length=255,
        error_messages={
            "required": "Vui lòng nhập địa chỉ giao hàng.",
            "blank": "Vui lòng nhập địa chỉ giao hàng.",
            "max_length": "Địa chỉ tối đa 255 ký tự.",
        },
    )
    payment_method = serializers.ChoiceField(
        choices=Order.PAYMENT_METHOD_CHOICES,
        error_messages={
            "required": "Vui lòng chọn phương thức thanh toán.",
            "invalid_choice": "Phương thức thanh toán không hợp lệ.",
        },
    )


class OrderItemSerializer(serializers.ModelSerializer):
    product_id = serializers.IntegerField(read_only=True)
    product_name = serializers.CharField(source="product.name", read_only=True)
    product_image_url = serializers.CharField(source="product.image_url", read_only=True)
    line_total_vnd = serializers.SerializerMethodField()

    class Meta:
        model = OrderItem
        fields = [
            "id", "product_id", "product_name", "product_image_url",
            "quantity", "unit_price_vnd", "line_total_vnd",
        ]

    def get_line_total_vnd(self, item):
        return item.unit_price_vnd * item.quantity


class PaymentSerializer(serializers.ModelSerializer):
    status_label = serializers.CharField(source="get_status_display", read_only=True)

    class Meta:
        model = Payment
        fields = ["method", "status", "status_label", "transaction_code", "amount_vnd", "paid_at"]


class StatusLogSerializer(serializers.ModelSerializer):
    status_label = serializers.CharField(source="get_status_display", read_only=True)

    class Meta:
        model = OrderStatusLog
        fields = ["status", "status_label", "at"]


class OrderSerializer(serializers.ModelSerializer):
    """Chi tiết một đơn."""
    status_label = serializers.CharField(source="get_status_display", read_only=True)
    payment_method_label = serializers.CharField(source="get_payment_method_display", read_only=True)
    items = OrderItemSerializer(many=True, read_only=True)
    payment = PaymentSerializer(read_only=True)
    status_logs = StatusLogSerializer(many=True, read_only=True)
    can_cancel = serializers.SerializerMethodField()
    can_pay = serializers.SerializerMethodField()

    class Meta:
        model = Order
        fields = [
            "id", "status", "status_label", "total_vnd",
            "shipping_name", "phone", "address",
            "payment_method", "payment_method_label", "created_at",
            "items", "payment", "status_logs", "can_cancel", "can_pay",
        ]

    def get_can_cancel(self, order):
        # Chỉ hủy được đơn đang chờ xác nhận và chưa trả tiền
        return order.status == "pending" and order.payment.status != "paid"

    def get_can_pay(self, order):
        return order.status != "cancelled" and order.payment.status == "unpaid"


class OrderListSerializer(serializers.ModelSerializer):
    """Một dòng trong danh sách đơn của tôi."""
    status_label = serializers.CharField(source="get_status_display", read_only=True)
    item_count = serializers.SerializerMethodField()
    payment_status = serializers.CharField(source="payment.status", read_only=True)
    payment_status_label = serializers.CharField(source="payment.get_status_display", read_only=True)

    class Meta:
        model = Order
        fields = [
            "id", "status", "status_label", "total_vnd", "created_at",
            "item_count", "payment_status", "payment_status_label",
        ]

    def get_item_count(self, order):
        return sum(item.quantity for item in order.items.all())
