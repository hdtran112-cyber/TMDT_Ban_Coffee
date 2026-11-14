from django.urls import path, include

urlpatterns = [
    path('api/auth/', include('accounts.urls')),
    path('api/', include('catalog.urls')),
    path('api/', include('roasting.urls')),
    path('api/', include('orders.urls')),
]
