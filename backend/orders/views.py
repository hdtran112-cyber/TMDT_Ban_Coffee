from django.db import transaction
from django.shortcuts import get_object_or_404
from rest_framework import status
from rest_framework.exceptions import ValidationError
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from catalog.models import Product
from .models import Cart, CartItem
from .serializers import (
    AddToCartSerializer,
    UpdateQuantitySerializer,
    cart_payload,
)


def get_cart(user):
    cart, _ = Cart.objects.get_or_create(user=user)
    return cart


def check_stock(product, quantity, in_cart=0):
    """Báo 400 nếu sản phẩm không bán được hoặc số lượng vượt tồn kho."""
    if not product.is_active:
        raise ValidationError({"product_id": ["Sản phẩm không còn được bán."]})
    if product.stock <= 0:
        raise ValidationError({"quantity": ["Sản phẩm đã hết hàng."]})
    if quantity > product.stock:
        if in_cart:
            msg = f"Bạn đã có {in_cart} trong giỏ, kho chỉ còn {product.stock}."
        else:
            msg = f"Kho chỉ còn {product.stock} sản phẩm."
        raise ValidationError({"quantity": [msg]})


class CartView(APIView):
    """GET /api/cart/: xem giỏ hàng của chính mình."""
    permission_classes = [IsAuthenticated]

    def get(self, request):
        return Response(cart_payload(get_cart(request.user)))


class CartItemListView(APIView):
    """POST /api/cart/items/: thêm sản phẩm vào giỏ (đã có thì cộng dồn)."""
    permission_classes = [IsAuthenticated]

    @transaction.atomic
    def post(self, request):
        serializer = AddToCartSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        quantity = serializer.validated_data["quantity"]

        product = Product.objects.filter(pk=serializer.validated_data["product_id"]).first()
        if product is None or not product.is_active:
            raise ValidationError({"product_id": ["Sản phẩm không tồn tại hoặc không còn được bán."]})

        cart = get_cart(request.user)
        item = cart.items.filter(product=product).first()
        in_cart = item.quantity if item else 0
        new_quantity = in_cart + quantity
        check_stock(product, new_quantity, in_cart)

        if item:
            item.quantity = new_quantity
            item.save(update_fields=["quantity"])
        else:
            CartItem.objects.create(cart=cart, product=product, quantity=new_quantity)
        return Response(cart_payload(cart), status=status.HTTP_201_CREATED)


class CartItemDetailView(APIView):
    """PATCH: đổi số lượng một dòng. DELETE: bỏ dòng khỏi giỏ."""
    permission_classes = [IsAuthenticated]

    def get_item(self, request, pk):
        # Lọc theo cart__user nên không ai đụng được vào giỏ của người khác (trả 404)
        return get_object_or_404(
            CartItem.objects.select_related("product", "cart"),
            pk=pk,
            cart__user=request.user,
        )

    def patch(self, request, pk):
        item = self.get_item(request, pk)
        serializer = UpdateQuantitySerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        quantity = serializer.validated_data["quantity"]

        check_stock(item.product, quantity)
        item.quantity = quantity
        item.save(update_fields=["quantity"])
        return Response(cart_payload(item.cart))

    def delete(self, request, pk):
        self.get_item(request, pk).delete()
        return Response(status=status.HTTP_204_NO_CONTENT)
