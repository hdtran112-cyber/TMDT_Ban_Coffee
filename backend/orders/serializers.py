from rest_framework import serializers

from catalog.models import Product
from .models import CartItem

QUANTITY_ERRORS = {
    "invalid": "Số lượng phải là số nguyên.",
    "min_value": "Số lượng phải từ 1 trở lên.",
}


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
