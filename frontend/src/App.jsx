import { useEffect, useState } from "react";
import { getProducts } from "./api";

export default function App() {
  const [products, setProducts] = useState([]);
  const [error, setError] = useState("");

  useEffect(() => {
    getProducts()
      .then(setProducts)
      .catch((e) => setError(e.message));
  }, []);

  return (
    <div style={{ maxWidth: 720, margin: "2rem auto", fontFamily: "sans-serif" }}>
      <h1>Cà phê rang</h1>
      {error && <p style={{ color: "crimson" }}>{error}</p>}
      <ul>
        {products.map((p) => (
          <li key={p.id}>
            {p.name} - {p.price_vnd.toLocaleString("vi-VN")}đ ({p.roast_level})
          </li>
        ))}
      </ul>
    </div>
  );
}
