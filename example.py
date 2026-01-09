from datetime import datetime

log_path = "D:/worklance/unix-crontab/hello.log"

with open(log_path, "a") as f:
    now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    f.write(f"{now} hello\n")
