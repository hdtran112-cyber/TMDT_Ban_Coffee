import { useEffect, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { getProduct } from "../../api/catalog";
import { ROAST_LABEL, formatPrice } from "./labels";

export default function ProductDetailPage() {
  const { id } = useParams();
  const [product, setProduct] = useState(null);
  const [error, setError] = useState("");

  useEffect(() => {
    getProduct(id)
      .then(setProduct)
      .catch((err) =>
        setError(err.status === 404 ? "Không tìm thấy sản phẩm." : "Không tải được sản phẩm.")
      );
  }, [id]);

  if (error) return <p className="msg-error">{error} <Link to="/">Quay lại danh sách</Link></p>;
  if (!product) return <p className="msg-muted">Đang tải...</p>;

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
          <p>{product.stock > 0 ? "Còn hàng" : "Hết hàng"}</p>
          <p className="msg-muted">{product.description}</p>
          <button className="btn" disabled title="Sẽ nối với giỏ hàng">Thêm vào giỏ</button>
        </div>
      </div>
    </div>
  );
}
