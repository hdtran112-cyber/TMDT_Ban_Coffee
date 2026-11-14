from django.urls import path, include

urlpatterns = [
    path('api/', include('catalog.urls')),
    path('api/', include('orders.urls')),
]
