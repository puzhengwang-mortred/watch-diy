# watch-diy 服务端（阶段 1：Caddy + 自签 TLS + `/health`）

与 [docs/05-cloud-and-app.md](../docs/05-cloud-and-app.md) 一致：公网 **HTTPS :18443**，证书 **SAN=服务器公网 IPv4**（不买域名方案）。

## 前提

- 云主机：**Linux**（Ubuntu 22.04 等），已装 **Docker** + **Docker Compose v2**（`docker compose`）。
- 安全组 / 防火墙：**放行入站 TCP `18443`**（仅先给你家固定 IP 或办公 IP 亦可）。
- 本机知道服务器的 **公网 IPv4**（与控制台「公网 IP」一致）。

## 1. 把本目录拷到服务器

任选其一：

```bash
# 从本仓库 clone 后只拷 server 目录
scp -r server/ user@YOUR_SERVER_IP:~/watch-diy-server/
ssh user@YOUR_SERVER_IP
cd ~/watch-diy-server
```

或直接在服务器上 `git clone` 整个仓库后 `cd watch-diy/server`。

## 2. 生成证书

在 **`server/`** 目录下执行（把 `x.x.x.x` 换成你的公网 IP）：

```bash
chmod +x scripts/gen-certs.sh
./scripts/gen-certs.sh x.x.x.x
# 非交互覆盖已有证书: OVERWRITE=1 ./scripts/gen-certs.sh x.x.x.x
```

生成文件在 **`caddy/certs/`**：

| 文件 | 用途 |
|------|------|
| `ca.crt` | 手表固件、家长 APP **内置信任的根证书**（只拷公钥） |
| `ca.key` | **勿上云仓库**；离线备份（U 盘 / 密码管理器） |
| `server.crt` / `server.key` | 仅 Caddy 加载 |

## 3. 启动 Caddy

```bash
docker compose up -d
docker compose logs -f caddy   # 可选：看日志
```

## 4. 本机验证

**先看你在哪台机器上跑 `curl`：**

- **在云服务器上**（已执行过 `./scripts/gen-certs.sh`，证书在本地磁盘）：先 `cd` 到 **`server/`**，相对路径才成立：

```bash
cd /path/to/server
curl -k https://x.x.x.x:18443/health

# 用 CA 校验（与固件/App 行为一致）
curl --cacert caddy/certs/ca.crt https://x.x.x.x:18443/health
```

- **在你自己的电脑（Windows / 从仓库 clone 下来的目录）上**：仓库默认**没有** `caddy/certs/ca.crt`（私钥与证书在 `.gitignore`），需要先用 **`scp`** 从服务器把 `ca.crt` 拷到本机任意路径，再 **`--cacert` 指向真实存在的文件**。例如先：`scp user@x.x.x.x:/path/on/server/caddy/certs/ca.crt .`，再：

```powershell
# PowerShell / cmd，与 ca.crt 所在目录一致时：
curl --cacert .\ca.crt https://x.x.x.x:18443/health
```

Windows 自带 **`curl.exe` 使用 Schannel** 时，可能对证书做**吊销检查**，自签/内网场景下会出现 **`curl: (60) schannel: ... CERT_TRUST_REVOCATION_STATUS_UNKNOWN`**。在校验仍使用你的 CA 的前提下，可加上 **`--ssl-no-revoke`**（仅跳过吊销检查，不跳过「是否信任该 CA」）：

```powershell
curl --ssl-no-revoke --cacert .\certs-from-server\ca.crt https://x.x.x.x:18443/health
```

若出现 **`curl: (77) schannel: failed to open CA file`**（Windows）：说明 **`--cacert` 路径下没有该文件**，或路径写错；改用**绝对路径**或确认已从服务器下载 `ca.crt`。

根路径：

```bash
curl -k https://x.x.x.x:18443/
# 应输出: watch-diy edge
```

## 5. 客户端预埋 `ca.crt`

- **ESP-IDF**：把服务器上的 **`ca.crt` 复制到** `firmware/main/certs/`，**改名为 `ca.pem`**（内容与 `ca.crt` 完全相同，仅扩展名；均为 PEM 文本）。嵌入方式见 [04-固件架构](../docs/04-firmware-architecture.md)。
- **Flutter**：`assets/certs/ca.pem`，[05 §7.5](../docs/05-cloud-and-app.md) `SecurityContext`。

**若云主机更换公网 IP**：重新运行 `./scripts/gen-certs.sh 新IP`，`docker compose restart caddy`，并把新手表/App 中证书或 pinning 更新（或续用同一 CA 只重签 server 证书，由运维脚本扩展）。

## 故障排查

| 现象 | 检查 |
|------|------|
| `/usr/bin/env: 'bash\r': No such file or directory` | 脚本为 **CRLF** 换行。在服务器：`sed -i 's/\r$//' scripts/gen-certs.sh` 后重跑；或 `git pull` 取回已强制 LF 的仓库。 |
| `curl: (77) schannel: failed to open CA file`（Windows） | **`ca.crt` 未拷到本机或路径错误**；从服务器 `scp` 后再 `--cacert`，见 **§4**。 |
| `curl: (60) schannel: ... REVOCATION_STATUS_UNKNOWN`（Windows） | Schannel 吊销检查失败；自签场景可 **`curl --ssl-no-revoke --cacert ...`**，见 **§4**。 |
| 拉镜像 **`docker.io` / `registry-1.docker.io` i/o timeout** | 多为访问 Docker Hub 受限。见下 **「Docker Hub 不可达」**；不打算买云侧代理仓时可 **本机拉取再 `docker load`**（同节）。 |
| 连接超时 | 云安全组是否放行 **18443**；本机 `ping` / `telnet IP 18443` |
| 证书错误 | `server.crt` 的 SAN 是否等于当前访问用的 **公网 IP**（不要用内网 IP 生成） |
| 404 | `Caddyfile` 是否挂载正确；`docker compose logs caddy` |

### Docker Hub 不可达（国内常见）

报错类似：`failed to resolve reference ... caddy:2-alpine` 或对 `registry-1.docker.io:443` **dial timeout**。镜像名仍是 `caddy:2-alpine`，通过 **Docker 守护进程镜像加速** 拉取即可。

1. **优先**：使用你云厂商控制台里的 **容器镜像加速器专属地址**（阿里云 ACR「镜像加速器」、腾讯云等均有；地址每人不同，比公共镜像站更稳）。
2. 在服务器编辑 **`/etc/docker/daemon.json`**（没有则新建；若已有其它配置，只合并 `registry-mirrors` 数组，勿覆盖整条 JSON）：

```json
{
  "registry-mirrors": [
    "https://<你的加速器地址>"
  ]
}
```

3. 重载 Docker 并重试拉镜：

```bash
systemctl daemon-reload
systemctl restart docker
cd /path/to/server
docker compose pull
docker compose up -d
```

公共镜像站 URL 经常变更；长期维护以 **云厂商加速器** 或自建/registry 代理为准。若服务器已能走稳定出境代理，也可在 Docker 层配置 `HTTP(S)_PROXY`（略）。

### 本机拉取再导入服务器（不付费远端代理仓）

在你**能访问 Docker Hub 的机器**（家里宽带、境外机、已配加速的本机等）执行：

```bash
docker pull caddy:2-alpine
docker save caddy:2-alpine -o caddy-2-alpine.tar
```

把 `caddy-2-alpine.tar` 传到云服务器（在**本机**执行，`user` / `IP` / 路径按你实际改）：

```bash
scp caddy-2-alpine.tar user@x.x.x.x:/tmp/
# 非默认 22 端口：
# scp -P 2222 caddy-2-alpine.tar user@x.x.x.x:/tmp/
```

或用 **`rsync -avP caddy-2-alpine.tar user@x.x.x.x:/tmp/`**（断点续传、看进度）。

在**服务器**上：

```bash
docker load -i /tmp/caddy-2-alpine.tar
cd /path/to/server
docker compose up -d --pull never
```

`--pull never` 避免 Compose 再访问 registry。以后换版本时在本机对新 tag 重复 **`pull` → `save` → `load`** 即可。

自建镜像同理：`docker build` 或 `docker pull` 后 **`docker save 镜像名:标签`**，到服务器 **`docker load`**。

## 后续阶段

增加 Go API、EMQX、PostgreSQL 时，扩展根目录 `docker-compose.yml` 并改 `Caddyfile` 中 `reverse_proxy`。当前 compose **仅含 Caddy**，无数据库依赖。
