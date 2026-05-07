# /// script
# dependencies = ["streamlit"]
# ///
import os
import sys

"""
Streamlit-интерфейс для проверки поискового индекса.

Вызывает скомпилированный C-бинарник ./app с разными бэкендами (avl, rb, btree).
C-программа должна выводить JSON в stdout:

    {
      "total": 47,
      "time_ms": 3.2,
      "results": [
        {"doc_id": 1234, "title": "...", "score": 3}
      ]
    }

Запуск:
    uv add streamlit
    streamlit run app.py
"""

import json
import subprocess
import time
from pathlib import Path

import streamlit as st  # type: ignore[import-untyped]

APP_BINARY = Path(__file__).parent / "app"

st.set_page_config(page_title="Stack Overflow Search", layout="wide")
st.title("Stack Overflow Search")
st.caption("Инвертированный индекс на AVL / Red-Black / B-tree")

col_left, col_right = st.columns([3, 1])

with col_left:
    query = st.text_input("Поисковый запрос", placeholder="python list sort")

with col_right:
    tree_type = st.selectbox("Структура данных", ["avl", "rb", "btree"])

search_clicked = st.button("Найти", use_container_width=True)

if search_clicked:
    if not query.strip():
        st.warning("Введите запрос")
    elif not APP_BINARY.exists():
        st.error(f"Бинарник не найден: {APP_BINARY}\nСоберите проект: `make app`")
    else:
        with st.spinner("Поиск..."):
            t0 = time.monotonic()
            proc = subprocess.run(
                [str(APP_BINARY), "search", f"--type={tree_type}", "--json", query],
                capture_output=True,
                text=True,
                timeout=30,
            )
            wall_ms = (time.monotonic() - t0) * 1000

        if proc.returncode != 0:
            st.error(f"Ошибка выполнения:\n```\n{proc.stderr}\n```")
        else:
            data: dict = {}
            try:
                data = json.loads(proc.stdout)
            except json.JSONDecodeError:
                st.error(
                    f"Не удалось распарсить вывод программы:\n```\n{proc.stdout[:500]}\n```"
                )
                st.stop()

            total   = data.get("total", 0)
            idx_ms  = data.get("time_ms", wall_ms)
            results = data.get("results", [])

            st.write(f"**Найдено:** {total} документов &nbsp;|&nbsp; **Время:** {idx_ms:.1f} мс")

            if not results:
                st.info("Ничего не найдено")
            else:
                for i, r in enumerate(results[:10], 1):
                    with st.expander(f"{i}. {r.get('title', '—')}"):
                        cols = st.columns([1, 1])
                        cols[0].write(f"**Doc ID:** {r.get('doc_id', '—')}")
                        cols[1].write(f"**Score:** {r.get('score', '—')}")
