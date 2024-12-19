const WebSocket = require("ws");
const express = require("express");
const path = require("path");
const http = require("http");
const mysql = require("mysql2");

const app = express();
const server = http.createServer(app);
const wss = new WebSocket.Server({ server }); // Gắn WebSocket vào server HTTP

app.use(express.json()); // Middleware để parse JSON
app.use(express.urlencoded({ extended: true })); // Middleware để parse form data

// Middleware để phục vụ file tĩnh từ thư mục 'cuatudong'
app.use(express.static(path.join(__dirname, "cuatudong")));

const db = mysql.createConnection({
  host: "localhost", // Thay bằng địa chỉ máy chủ cơ sở dữ liệu
  user: "root", // Thay bằng tên người dùng của bạn
  password: "", // Thay bằng mật khẩu của bạn
  database: "cuatudong", // Thay bằng tên cơ sở dữ liệu của bạn
});

// Hiển thị dangnhap.html khi khởi động
app.get("/", (req, res) => {
  res.sendFile(path.join(__dirname, "cuatudong", "dangnhap.html"));
});

// API xử lý đăng nhập
app.post("/login", (req, res) => {
  const { username, password } = req.body;

  const query = "SELECT * FROM user WHERE username = ? AND password = ?";
  db.query(query, [username, password], (err, results) => {
    if (err) {
      console.error("Database query error:", err.message);
      return res.status(500).send("<h1>Lỗi cơ sở dữ liệu. Vui lòng thử lại sau!</h1>");
    }

    if (results.length > 0) {
      // Kiểm tra quyền người dùng
      switch (username) {
        case "admin":
          res.redirect("/admin.html");
          break;
        case "user":
          res.redirect("/user.html");
          break;
        default:
          res.send("<h1>Đăng nhập thất bại. Vui lòng thử lại!</h1>");
      }
    } else {
      res.send("<h1>Đăng nhập thất bại. Vui lòng thử lại!</h1>");
    }
  });
});
let lastMessage = ""; // Biến lưu trữ dữ liệu cũ

// Kết nối đến cơ sở dữ liệu
db.connect((err) => {
  if (err) {
    console.error("Error connecting to the database:", err);
    return;
  }
  console.log("Connected to the database");
});

// Hàm kiểm tra và chuyển tiếp dữ liệu
function checkAndForwardMessage(ws, decodedMessage) {
  wss.clients.forEach((client) => {
    if (client !== ws && client.readyState === WebSocket.OPEN) {
      console.log(`Forwarding message to client: ${decodedMessage}`);
      client.send(decodedMessage);
    }
  });
}

// Hàm xử lý dữ liệu từ ESP8266
function handleESPMessage(message) {
  console.log("Received data from ESP8266:", message);

  // Điều chỉnh theo nhu cầu gửi dữ liệu đến những client cụ thể
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      // Giới hạn việc gửi tới các client đặc biệt, tránh gây xung đột
      console.log(`Sending ESP data to client: ${message}`);
      client.send(`ESP: ${message}`);
    }
  });
}

// Hàm insert vào bảng 'manual'
function insertManual(ws, key, value, temperature, humidity) {
  const sql = "INSERT INTO thongtincua (trangthaicua, thoigiandongmo, nhietdo ,doam) VALUES (?, ?, ?, ?)";
  db.query(sql, [key, value, temperature, humidity], (err, result) => {
    if (err) {
      console.error("Error inserting data:", err);
      return;
    }
    fetchManualData((data) => {
      data.forEach((row) => {
        const message = {
          timestamp: row.thoigian || "N/A",
          door_state: row.trangthaicua || "N/A",
          time_between: row.thoigiandongmo || "N/A",
        };
        ws.send(JSON.stringify(message));
      });
    });
    console.log("Data inserted successfully:", result);
  });
}

// Hàm select từ bảng 'manual'
function fetchManualData(callback) {
  const sql = "SELECT * FROM thongtincua WHERE trangthaicua = 'open' OR trangthaicua = 'close'";
  db.query(sql, (err, results) => {
    if (err) {
      console.error("Error fetching data:", err);
      return;
    }
    callback(results);
  });
}

// Hàm select trạng thái cửa gần nhất
function fetchLatestDoorState(callback) {
  const sql = "SELECT trangthaicua FROM thongtincua ORDER BY thoigian DESC LIMIT 1";
  db.query(sql, (err, results) => {
    if (err) {
      console.error("Error fetching latest door state:", err);
      return;
    }
    if (results.length > 0) {
      callback(results[0]);
    } else {
      callback(null);
    }
  });
}

// Sự kiện WebSocket
wss.on("connection", (ws) => {
  console.log("Client connected");

  fetchManualData((data) => {
    data.forEach((row) => {
      const message = {
        timestamp: row.thoigian || "N/A",
        door_state: row.trangthaicua || "N/A",
        time_between: row.thoigiandongmo || "N/A",
      };
      ws.send(JSON.stringify(message));
    });
  });

  ws.on("message", (message) => {
    const decodedMessage = message.toString();
    console.log("Received from client:", decodedMessage);

    // Gọi hàm kiểm tra và chuyển tiếp dữ liệu
    checkAndForwardMessage(ws, decodedMessage);

    const [key, value, temperature, humidity] = decodedMessage.split(":");
    if (key && value && temperature && humidity) {
      if (key.includes("close") || key.includes("open")) {
        insertManual(ws, key, value, temperature, humidity);
        console.log("Key:", key);
        console.log("Value:", value);
        console.log("temperature:", temperature);
        console.log("humidity:", humidity);
      }
      if (key.includes("setting")) {
        const config = JSON.parse(decodedMessage);
        // Truy cập các giá trị
        const openDelay = config.openDelay;
        const closeDelay = config.closeDelay;
        const speed = config.speed;
        const settings = "setting_door:" + openDelay.toString() + ":" + closeDelay.toString() + ":" + speed.toString();
        wss.clients.forEach((client) => {
          if (client !== ws && client.readyState === WebSocket.OPEN) {
            console.log(`Forwarding message to client: ${settings}`);
            client.send(JSON.stringify(settings));
          }
        });
      }
    }

    // Nếu message chứa "manual" hoặc "auto"
    if (decodedMessage.includes("manual") || decodedMessage.includes("auto")) {
      fetchLatestDoorState((latestState) => {
        console.log("Fetched latest door state:", latestState);
        if (latestState) {
          const latestMessage = {
            laststate: `${latestState.trangthaicua}`,
          };
          wss.clients.forEach((client) => {
            if (client !== ws && client.readyState === WebSocket.OPEN) {
              console.log("Sending to ESP:", JSON.stringify(latestMessage));
              console.log(`Forwarding message to client: ${latestMessage}`);
              client.send(JSON.stringify(latestMessage));
            }
          });
        } else {
          console.log("No recent door state found.");
          ws.send("No recent door state found.");
        }
      });
    }

    // Nếu dữ liệu đến từ ESP8266, gọi hàm xử lý riêng
    if (decodedMessage.startsWith("ESP:")) {
      // Xử lý riêng biệt cho dữ liệu ESP8266
      handleESPMessage(decodedMessage.replace("ESP:", "").trim());
      console.log("Processed ESP message:", decodedMessage);
    }
  });

  ws.on("close", () => {
    console.log("Client disconnected");
  });

  ws.send("Hello from Node.js Server");
});

server.listen(8080, () => {
  console.log("Server is running on http://localhost:8080");
});
