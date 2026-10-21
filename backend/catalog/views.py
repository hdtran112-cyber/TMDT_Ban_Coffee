from rest_framework import generics
from rest_framework.exceptions import ValidationError
from .models import Product
from .serializers import ProductSerializer

ALLOWED_ORDERING = {"price_vnd", "-price_vnd", "-created_at", "name"}


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

        for key, lookup in (("price_min", "price_vnd__gte"), ("price_max", "price_vnd__lte")):
            value = params.get(key)
            if value:
                try:
                    qs = qs.filter(**{lookup: int(value)})
                except ValueError:
                    raise ValidationError({key: "Phải là số nguyên"})

        ordering = params.get("ordering")
        if ordering:
            if ordering not in ALLOWED_ORDERING:
                raise ValidationError({"ordering": "Giá trị không hợp lệ"})
            qs = qs.order_by(ordering)

        return qs


class ProductDetailView(generics.RetrieveAPIView):
    queryset = Product.objects.filter(is_active=True)
    serializer_class = ProductSerializer
