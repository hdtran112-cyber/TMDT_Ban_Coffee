from django.urls import path

from .views import ProductListView, ProductDetailView
from .admin_views import (
    AdminProductListCreateView,
    AdminProductDetailView,
)

urlpatterns = [
    path("products/", ProductListView.as_view(), name="product-list"),
    path(
        "products/<int:pk>/",
        ProductDetailView.as_view(),
        name="product-detail",
    ),
    path(
        "admin/products/",
        AdminProductListCreateView.as_view(),
        name="admin-product-list-create",
    ),
    path(
        "admin/products/<int:pk>/",
        AdminProductDetailView.as_view(),
        name="admin-product-detail",
    ),
]
