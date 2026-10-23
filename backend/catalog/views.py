from rest_framework import generics
from rest_framework.exceptions import ValidationError
from .models import Product
from .serializers import ProductSerializer

ALLOWED_ORDERING = {"price_vnd", "-price_vnd", "-created_at", "name"}


def parse_price(params, key):
    """Đọc một tham số giá; trả None nếu không gửi, báo 400 nếu sai."""
    val = params.get(key)
    if val in (None, ""):
        return None
    try:
        n = int(val)
    except ValueError:
        raise ValidationError({key: "Phải là số nguyên."})
    if n < 0:
        raise ValidationError({key: "Không được là số âm."})
    return n


class ProductListView(generics.ListAPIView):
    serializer_class = ProductSerializer

    def get_queryset(self):
        params = self.request.query_params
        qs = Product.objects.filter(is_active=True)

        q = params.get("q")
        if q:
            qs = qs.filter(name__icontains=q)

        roast = params.get("roast_level")
        if roast:
            qs = qs.filter(roast_level=roast)

        bean_type = params.get("bean_type")
        if bean_type:
            qs = qs.filter(bean_type__icontains=bean_type)

        price_min = parse_price(params, "price_min")
        price_max = parse_price(params, "price_max")
        if price_min is not None and price_max is not None and price_min > price_max:
            raise ValidationError({"price_max": "Giá tối đa không được nhỏ hơn giá tối thiểu."})
        if price_min is not None:
            qs = qs.filter(price_vnd__gte=price_min)
        if price_max is not None:
            qs = qs.filter(price_vnd__lte=price_max)

        ordering = params.get("ordering")
        if ordering in ALLOWED_ORDERING:
            qs = qs.order_by(ordering)

        return qs


class ProductDetailView(generics.RetrieveAPIView):
    queryset = Product.objects.filter(is_active=True)
    serializer_class = ProductSerializer
