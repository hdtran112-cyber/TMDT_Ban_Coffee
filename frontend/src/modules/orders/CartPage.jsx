import { useEffect, useState } from "react";
import { Link } from "react-router-dom";
import { getCart, removeCartItem, updateCartItem } from "../../api/cart";
import { ROAST_LABEL, formatPrice } from "../catalog/labels";
import { apiErrorText } from "./errors";
import "./cart.css";

export default function CartPage() {
  const [cart, setCart] = useState(null);
  const [loading, setLoading] = useState(true);
  const [needLogin, setNeedLogin] = useState(false);
  const [error, setError] = useState("");
  const [busyId, setBusyId] = useState(null); // dòng đang được cập nhật

  // Vào trang: chưa có token thì khỏi gọi API, bắt đăng nhập luôn
  useEffect(() => {
    if (!localStorage.getItem("token")) {
      setNeedLogin(true);
      setLoading(false);
      return;
    }
    getCart()
      .then(setCart)
      .catch((err) => handleError(err, "Không tải được giỏ hàng."))
      .finally(() => setLoading(false));
  }, []);

  function handleError(err, fallback) {
    console.error("Lỗi giỏ hàng:", err);
    if (err.status === 401) setNeedLogin(true);
    else setError(apiErrorText(err, fallback));
  }

  async function changeQuantity(item, quantity) {
    setBusyId(item.id);
    setError("");
    try {
      setCart(await updateCartItem(item.id, quantity));
    } catch (err) {
      handleError(err, "Không đổi được số lượng.");
    } finally {
      setBusyId(null);
    }
  }

  async function remove(item) {
    setBusyId(item.id);
    setError("");
    try {
      await removeCartItem(item.id);
      setCart(await getCart()); // tải lại để lấy tổng tiền mới từ backend
    } catch (err) {
      handleError(err, "Không xóa được sản phẩm.");
    } finally {
      setBusyId(null);
    }
  }

  if (loading) return <p className="msg-muted">Đang tải...</p>;

  if (needLogin) {
    return (
      <div>
        <h1 className="page-title">Giỏ hàng</h1>
        <p className="msg-muted">Bạn cần đăng nhập để xem giỏ hàng.</p>
        <Link to="/login?next=/cart" className="btn">Đăng nhập</Link>
      </div>
    );
  }

  if (!cart) return <p className="msg-error">{error || "Không tải được giỏ hàng."}</p>;

  const hasProblem = cart.items.some((item) => !item.available);

  return (
    <div>
      <h1 className="page-title">Giỏ hàng</h1>
      <p className="page-sub">{cart.items.length} sản phẩm trong giỏ</p>

      {error && <p className="msg-error">{error}</p>}

      {cart.items.length === 0 ? (
        <div>
          <p className="msg-muted">Giỏ hàng đang trống.</p>
          <Link to="/" className="btn">Xem sản phẩm</Link>
        </div>
      ) : (
        <>
          <div className="cart-list">
            {cart.items.map((item) => {
              const p = item.product;
              const busy = busyId === item.id;
              return (
                <div key={item.id} className="cart-row">
                  <div className={`thumb ${p.roast_level}`}>
                    {p.image_url ? <img src={p.image_url} alt={p.name} /> : ROAST_LABEL[p.roast_level]}
                  </div>

                  <div className="cart-info">
                    <Link to={`/products/${p.id}`} className="cart-name">{p.name}</Link>
                    <span className="card-meta">{p.weight_g}g · {formatPrice(p.price_vnd)}</span>
                    {!item.available && (
                      <span className="msg-error">
                        Sản phẩm không đủ hàng hoặc đã ngừng bán. Hãy giảm số lượng hoặc xóa khỏi giỏ.
                      </span>
                    )}
                  </div>

                  <div className="qty">
                    <button
                      type="button"
                      aria-label="Giảm số lượng"
                      disabled={busy || item.quantity <= 1}
                      onClick={() => changeQuantity(item, item.quantity - 1)}
                    >
                      −
                    </button>
                    <span>{item.quantity}</span>
                    <button
                      type="button"
                      aria-label="Tăng số lượng"
                      disabled={busy || item.quantity >= p.stock}
                      onClick={() => changeQuantity(item, item.quantity + 1)}
                    >
                      +
                    </button>
                  </div>

                  <span className="cart-line-total">{formatPrice(item.line_total_vnd)}</span>

                  <button
                    type="button"
                    className="btn btn-ghost"
                    disabled={busy}
                    onClick={() => remove(item)}
                  >
                    Xóa
                  </button>
                </div>
              );
            })}
          </div>

          <div className="cart-summary">
            <div>
              <span className="msg-muted">Tổng cộng</span>
              <div className="cart-total">{formatPrice(cart.total_vnd)}</div>
            </div>
            <div className="actions">
              <Link to="/" className="btn btn-ghost">← Tiếp tục mua sắm</Link>
              {hasProblem ? (
                <button
                  type="button"
                  className="btn"
                  disabled
                  title="Hãy xử lý sản phẩm báo lỗi trước"
                >
                  Đặt hàng
                </button>
              ) : (
                <Link to="/checkout" className="btn">Đặt hàng</Link>
              )}
            </div>
          </div>
        </>
      )}
    </div>
  );
}
