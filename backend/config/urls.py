from django.urls import include, path

urlpatterns = [
    path("api/auth/", include("user_accounts.urls")),
    path("api/", include("catalog.urls")),
]