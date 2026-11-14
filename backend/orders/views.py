import uuid

from django.db import transaction
from django.db.models import F
from django.shortcuts import get_object_or_404
from django.utils import timezone
from rest_framework import status
from rest_framework.exceptions import ValidationError
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from catalog.models import Product
from .models import Cart, CartItem, Order, OrderItem, OrderStatusLog, Payment
from .serializers import (
    AddToCartSerializer,
    CheckoutSerializer,
    OrderListSerializer,
    OrderSerializer,
    UpdateQuantitySerializer,
    cart_payload,
)
from .signals import order_created


# ---------- Giỏ hàng ----------

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


# ---------- Đơn hàng ----------

def user_orders(user):
    """Các đơn của một người, kèm sẵn dữ liệu liên quan để đỡ truy vấn lặp."""
    return (
        Order.objects.filter(user=user)
        .select_related("payment")
        .prefetch_related("items__product", "status_logs")
    )


def order_response(user, pk):
    """Đọc lại đơn từ CSDL rồi trả JSON chi tiết."""
    order = user_orders(user).get(pk=pk)
    return Response(OrderSerializer(order).data)


class OrderListCreateView(APIView):
    """GET /api/orders/: đơn của tôi. POST /api/orders/: đặt hàng từ giỏ."""
    permission_classes = [IsAuthenticated]

    def get(self, request):
        orders = user_orders(request.user)
        return Response(OrderListSerializer(orders, many=True).data)

    @transaction.atomic
    def post(self, request):
        # Mọi bước nằm trong một giao dịch: lỗi giữa chừng thì không có gì bị lưu
        serializer = CheckoutSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        cart = get_cart(request.user)
        cart_items = list(cart.items.all())
        if not cart_items:
            raise ValidationError({"cart": ["Giỏ hàng đang trống."]})

        # Khóa các dòng sản phẩm để hai người mua cùng lúc không làm kho bị âm
        products = {
            p.pk: p
            for p in Product.objects.select_for_update().filter(
                pk__in=[item.product_id for item in cart_items]
            )
        }

        problems = []
        for item in cart_items:
            product = products[item.product_id]
            if not product.is_active:
                problems.append(f"{product.name}: không còn được bán.")
            elif product.stock < item.quantity:
                problems.append(f"{product.name}: kho chỉ còn {product.stock}.")
        if problems:
            raise ValidationError({"cart": problems})

        # Tổng tiền tính lại từ giá hiện tại trong CSDL, không tin dữ liệu từ client
        total = sum(products[item.product_id].price_vnd * item.quantity for item in cart_items)

        order = Order.objects.create(
            user=request.user,
            total_vnd=total,
            **serializer.validated_data,
        )
        for item in cart_items:
            product = products[item.product_id]
            OrderItem.objects.create(
                order=order,
                product=product,
                quantity=item.quantity,
                unit_price_vnd=product.price_vnd,
            )
            Product.objects.filter(pk=product.pk).update(stock=F("stock") - item.quantity)

        Payment.objects.create(
            order=order,
            method=order.payment_method,
            status="unpaid",
            amount_vnd=total,
        )
        OrderStatusLog.objects.create(order=order, status="pending")
        cart.items.all().delete()

        # Phát sự kiện sau khi giao dịch đã lưu xong
        transaction.on_commit(lambda: order_created.send(sender=Order, order=order))

        response = order_response(request.user, order.pk)
        response.status_code = status.HTTP_201_CREATED
        return response


class OrderDetailView(APIView):
    """GET /api/orders/<id>/: chi tiết đơn. Chỉ xem được đơn của chính mình."""
    permission_classes = [IsAuthenticated]

    def get(self, request, pk):
        order = get_object_or_404(user_orders(request.user), pk=pk)
        return Response(OrderSerializer(order).data)


class OrderCancelView(APIView):
    """POST /api/orders/<id>/cancel/: hủy đơn đang chờ xác nhận, trả lại hàng vào kho."""
    permission_classes = [IsAuthenticated]

    @transaction.atomic
    def post(self, request, pk):
        order = get_object_or_404(
            Order.objects.select_for_update().filter(user=request.user), pk=pk
        )
        if order.status != "pending":
            raise ValidationError({"detail": ["Chỉ hủy được đơn đang chờ xác nhận."]})
        if order.payment.status == "paid":
            raise ValidationError(
                {"detail": ["Đơn đã thanh toán, hãy liên hệ cửa hàng để được hoàn tiền."]}
            )

        for item in order.items.all():
            Product.objects.filter(pk=item.product_id).update(stock=F("stock") + item.quantity)

        order.status = "cancelled"
        order.save(update_fields=["status"])
        OrderStatusLog.objects.create(order=order, status="cancelled")
        return order_response(request.user, order.pk)


class OrderPayView(APIView):
    """POST /api/orders/<id>/pay/: thanh toán giả lập, không có cổng thanh toán thật."""
    permission_classes = [IsAuthenticated]

    @transaction.atomic
    def post(self, request, pk):
        order = get_object_or_404(
            Order.objects.select_for_update().filter(user=request.user), pk=pk
        )
        if order.status == "cancelled":
            raise ValidationError({"detail": ["Đơn đã hủy, không thể thanh toán."]})

        payment = order.payment
        if payment.status == "paid":
            raise ValidationError({"detail": ["Đơn này đã được thanh toán rồi."]})

        payment.status = "paid"
        payment.transaction_code = f"TXN{uuid.uuid4().hex[:12].upper()}"
        payment.paid_at = timezone.now()
        payment.save(update_fields=["status", "transaction_code", "paid_at"])
        return order_response(request.user, order.pk)
