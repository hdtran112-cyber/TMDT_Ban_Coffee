from django.urls import path

from .views import CartItemDetailView, CartItemListView, CartView

urlpatterns = [
    path("cart/", CartView.as_view()),
    path("cart/items/", CartItemListView.as_view()),
    path("cart/items/<int:pk>/", CartItemDetailView.as_view()),
]
