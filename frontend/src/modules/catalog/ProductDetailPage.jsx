import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { addToCart } from "../../api/cart";
import { getProduct } from "../../api/catalog";
import { apiErrorText } from "../orders/errors";
import { ROAST_LABEL, formatPrice } from "./labels";

export default function ProductDetailPage() {
  const { id } = useParams();
  const [product, setProduct] = useState(null);
  const [error, setError] = useState("");
  const [qty, setQty] = useState(1);
  const [adding, setAdding] = useState(false);
  const [notice, setNotice] = useState(null); // { type: "ok" | "error" | "login", text }

  useEffect(() => {
    setQty(1);
    setNotice(null);
    getProduct(id)
      .then(setProduct)
      .catch((err) =>
        setError(err.status === 404 ? "Không tìm thấy sản phẩm." : "Không tải được sản phẩm.")
      );
  }, [id]);

  async function onAdd() {
    setAdding(true);
    setNotice(null);
    try {
      await addToCart(product.id, qty);
      setNotice({ type: "ok", text: `Đã thêm ${qty} sản phẩm vào giỏ.` });
    } catch (err) {
      if (err.status === 401) setNotice({ type: "login" });
      else setNotice({ type: "error", text: apiErrorText(err, "Không thêm được vào giỏ.") });
    } finally {
      setAdding(false);
    }
  }

  if (error) return <p className="msg-error">{error} <Link to="/">Quay lại danh sách</Link></p>;
  if (!product) return <p className="msg-muted">Đang tải...</p>;

  const inStock = product.stock > 0;

  return (
    <div>
      <Link to="/" className="back">← Quay lại danh sách</Link>
      <div className="detail">
        <div className={`thumb ${product.roast_level}`}>
          {product.image_url ? <img src={product.image_url} alt={product.name} /> : ROAST_LABEL[product.roast_level]}
        </div>
        <div>
          <h1>{product.name}</h1>
          <p className="price">{formatPrice(product.price_vnd)} <span className="msg-muted">/ {product.weight_g}g</span></p>
          <p><span className="tag">{product.bean_type}</span> <span className="tag">{ROAST_LABEL[product.roast_level]}</span></p>
          <p>{inStock ? "Còn hàng" : "Hết hàng"}</p>
          <p className="msg-muted">{product.description}</p>

          <div className="add-to-cart">
            {inStock && (
              <input
                type="number"
                min="1"
                max={product.stock}
                value={qty}
                aria-label="Số lượng"
                onChange={(e) => setQty(Math.max(1, Number(e.target.value) || 1))}
              />
            )}
            <button className="btn" disabled={!inStock || adding} onClick={onAdd}>
              {adding ? "Đang thêm..." : "Thêm vào giỏ"}
            </button>
          </div>

          {notice?.type === "ok" && (
            <p className="msg-ok">{notice.text} <Link to="/cart">Xem giỏ hàng</Link></p>
          )}
          {notice?.type === "error" && <p className="msg-error">{notice.text}</p>}
          {notice?.type === "login" && (
            <p className="msg-error">Bạn cần đăng nhập để thêm vào giỏ. <Link to="/login">Đăng nhập</Link></p>
          )}
        </div>
      </div>
    </div>
  );
}
