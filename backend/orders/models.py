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
