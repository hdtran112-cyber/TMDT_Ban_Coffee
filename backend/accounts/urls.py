from django.urls import path
from rest_framework.authtoken.views import obtain_auth_token

from .views import RegisterView, CurrentUserView

urlpatterns = [
    path("register/", RegisterView.as_view(), name="auth-register"),
    path("login/", obtain_auth_token, name="auth-login"),
    path("me/", CurrentUserView.as_view(), name="auth-me"),
]
