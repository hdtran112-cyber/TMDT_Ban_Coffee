// Trang đăng nhập / đăng ký (dựa trên AuthPage của Anh ở nhánh feature/auth-frontend,
// chuyển sang dùng api/auth.js và khung chung của nhóm).
import { useState } from "react";
import { Link, useNavigate, useSearchParams } from "react-router-dom";
import { login, register } from "../../api/auth";
import { apiErrorText, fieldErrors } from "../orders/errors";
import "./accounts.css";

export default function AuthPage({ mode }) {
  const isRegister = mode === "register";
  const navigate = useNavigate();
  const [params] = useSearchParams();
  // ?next=/cart: đăng nhập xong quay lại đúng trang đang xem
  const next = params.get("next") || "/";

  const [form, setForm] = useState({ username: "", email: "", password: "", confirm: "" });
  const [errors, setErrors] = useState({});
  const [formError, setFormError] = useState("");
  const [message, setMessage] = useState("");
  const [submitting, setSubmitting] = useState(false);

  const onChange = (e) => setForm({ ...form, [e.target.name]: e.target.value });

  async function onSubmit(e) {
    e.preventDefault();
    setErrors({});
    setFormError("");
    setMessage("");

    if (isRegister && form.password !== form.confirm) {
      setErrors({ confirm: "Mật khẩu nhập lại không khớp." });
      return;
    }

    setSubmitting(true);
    const username = form.username.trim();
    try {
      if (isRegister) {
        await register(username, form.email.trim(), form.password);
        // Đăng ký xong thì đăng nhập luôn cho tiện
        await login(username, form.password);
        setMessage("Tạo tài khoản thành công.");
      } else {
        await login(username, form.password);
      }
      navigate(next, { replace: true });
    } catch (err) {
      // Đăng nhập sai trả về {non_field_errors: [...]} bằng tiếng Anh
      if (!isRegister && err.status === 400) {
        setFormError("Sai tên đăng nhập hoặc mật khẩu.");
      } else {
        setErrors(fieldErrors(err));
        setFormError(err.status === 400 ? "" : apiErrorText(err, "Không kết nối được máy chủ."));
      }
    } finally {
      setSubmitting(false);
    }
  }

  const switchTo = `${isRegister ? "/login" : "/register"}${next !== "/" ? `?next=${encodeURIComponent(next)}` : ""}`;

  return (
    <section className="auth-card">
      <h1 className="page-title">{isRegister ? "Tạo tài khoản" : "Đăng nhập"}</h1>
      <p className="auth-sub">
        {isRegister ? "Đăng ký để đặt hàng và theo dõi đơn." : "Đăng nhập để dùng giỏ hàng và đặt hàng."}
      </p>

      <form className="auth-form" onSubmit={onSubmit} noValidate>
        <Field label="Tên đăng nhập" error={errors.username}>
          <input name="username" autoComplete="username" value={form.username} onChange={onChange} required />
        </Field>
        {isRegister && (
          <Field label="Email" error={errors.email}>
            <input name="email" type="email" autoComplete="email" value={form.email} onChange={onChange} required />
          </Field>
        )}
        <Field label="Mật khẩu" error={errors.password}>
          <input
            name="password"
            type="password"
            autoComplete={isRegister ? "new-password" : "current-password"}
            value={form.password}
            onChange={onChange}
            required
          />
        </Field>
        {isRegister && (
          <Field label="Nhập lại mật khẩu" error={errors.confirm}>
            <input name="confirm" type="password" autoComplete="new-password" value={form.confirm} onChange={onChange} required />
          </Field>
        )}

        {formError && <p className="msg-error" role="alert">{formError}</p>}
        {message && <p className="msg-ok" role="status">{message}</p>}

        <button className="btn" type="submit" disabled={submitting}>
          {submitting ? "Đang xử lý..." : isRegister ? "Đăng ký" : "Đăng nhập"}
        </button>
      </form>

      <p className="auth-switch">
        {isRegister ? "Đã có tài khoản? " : "Chưa có tài khoản? "}
        <Link to={switchTo}>{isRegister ? "Đăng nhập" : "Đăng ký ngay"}</Link>
      </p>
    </section>
  );
}

function Field({ label, error, children }) {
  return (
    <label className="auth-field">
      <span>{label}</span>
      {children}
      {error && <small className="msg-error">{error}</small>}
    </label>
  );
}
