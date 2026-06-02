"""
Shut-the-fuck-up 控制服务器

供 Windows 客户端轮询状态，供 Android App 远程下发指令。
默认监听 0.0.0.0:8000，部署时请放行防火墙/安全组对应端口。

启动:
    pip install -r requirements.txt
    python server.py
"""

from flask import Flask, jsonify, request

app = Flask(__name__)

# 全局状态（内存存储，重启后恢复默认）
state = "stop"      # "start" | "stop"
interval = 30       # 每次联网窗口时长（秒）


@app.route("/get_state")
def get_state():
    """Windows 客户端 / Android 读取当前运行状态。"""
    return jsonify({"state": state})


@app.route("/get_interval")
def get_interval():
    """Windows 客户端 / Android 读取联网间隔（秒）。"""
    return jsonify({"interval": interval})


@app.route("/set_state", methods=["POST"])
def set_state():
    """
    Android 控制开/关。
    支持两种方式（任选其一）:
      - URL 参数: POST /set_state?state=start
      - JSON Body: {"state": "start"}
    """
    global state
    data = request.get_json(silent=True) or {}
    new_state = data.get("state") or request.args.get("state", "")
    if new_state in ("start", "stop"):
        state = new_state
        return jsonify({"msg": "success", "state": state})
    return jsonify({"msg": "error"}), 400


@app.route("/set_interval", methods=["POST"])
def set_interval():
    """
    Android 设置联网间隔。
    支持:
      - URL 参数: POST /set_interval?interval=30
      - JSON Body: {"interval": 30}
    """
    global interval
    data = request.get_json(silent=True) or {}
    raw = data.get("interval", request.args.get("interval", None))
    if raw is None:
        return jsonify({"msg": "error"}), 400
    try:
        interval = max(1, int(raw))
        return jsonify({"msg": "success", "interval": interval})
    except (TypeError, ValueError):
        return jsonify({"msg": "error"}), 400


if __name__ == "__main__":
    # host=0.0.0.0 允许外网访问；生产环境建议配合 systemd / nginx 使用
    app.run(host="0.0.0.0", port=8000, debug=False)
