FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential make gcc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY pyproject.toml uv.lock ./
RUN pip install --no-cache-dir uv
RUN uv sync --frozen

COPY . .

RUN make clean && make app

EXPOSE 8501

CMD ["uv", "run", "streamlit", "run", "app.py", "--server.address=0.0.0.0", "--server.port=8501"]
