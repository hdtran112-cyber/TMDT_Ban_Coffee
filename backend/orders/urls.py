from django.urls import path

from .views import (
    CartItemDetailView,
    CartItemListView,
    CartView,
    OrderCancelView,
    OrderDetailView,
    OrderListCreateView,
    OrderPayView,
)

urlpatterns = [
    path("cart/", CartView.as_view()),
    path("cart/items/", CartItemListView.as_view()),
    path("cart/items/<int:pk>/", CartItemDetailView.as_view()),
    path("orders/", OrderListCreateView.as_view()),
    path("orders/<int:pk>/", OrderDetailView.as_view()),
    path("orders/<int:pk>/cancel/", OrderCancelView.as_view()),
    path("orders/<int:pk>/pay/", OrderPayView.as_view()),
]
