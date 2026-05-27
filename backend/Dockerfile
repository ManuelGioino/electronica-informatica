FROM python:3.11-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY timbre_notificador.py .
CMD ["python", "-u", "timbre_notificador.py"]
