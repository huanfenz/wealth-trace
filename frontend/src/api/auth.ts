// 鉴权 API 与本地会话存储：登录令牌保存在 localStorage（90 天滑动有效期，
// 后端每次请求自动续期），只要 90 天内使用过一次就无需重新登录。
import { get, post } from './http'
import { SESSION_TOKEN_KEY } from './http'

const usernameKey = 'wt_session_username'

/** 后端 /api/auth 原始会话结构。 */
interface RawSession {
  token: string
  expires_at: number
  user: { id: number; username: string }
}

/** 登录会话信息。 */
export interface AuthSession {
  token: string
  expiresAt: number
  username: string
}

/** 鉴权状态：enabled=false 表示后端关闭了鉴权（本地开发模式）。 */
export interface AuthStatus {
  enabled: boolean
  initialized: boolean
}

export function getSessionToken(): string {
  return localStorage.getItem(SESSION_TOKEN_KEY) ?? ''
}

export function getSessionUsername(): string {
  return localStorage.getItem(usernameKey) ?? ''
}

export function hasSession(): boolean {
  return getSessionToken() !== ''
}

function saveSession(session: AuthSession) {
  localStorage.setItem(SESSION_TOKEN_KEY, session.token)
  localStorage.setItem(usernameKey, session.username)
}

function toSession(raw: RawSession): AuthSession {
  const session: AuthSession = {
    token: raw.token,
    expiresAt: raw.expires_at,
    username: raw.user?.username ?? '',
  }
  saveSession(session)
  return session
}

/** 查询鉴权状态（公开端点）。 */
export function getAuthStatus(): Promise<AuthStatus> {
  return get<AuthStatus>('/auth/status')
}

/** 首次创建管理员账号，成功即视为登录并缓存会话。 */
export function setupAccount(username: string, password: string): Promise<AuthSession> {
  return post<RawSession>('/auth/setup', { username, password }).then(toSession)
}

/** 登录并缓存会话令牌。 */
export function login(username: string, password: string): Promise<AuthSession> {
  return post<RawSession>('/auth/login', { username, password }).then(toSession)
}

/** 登出（吊销当前会话）；无论后端是否可达都清空本地令牌。 */
export async function logout(): Promise<void> {
  try {
    await post('/auth/logout', {})
  } catch {
    // 会话已过期或后端不可达：本地照常登出。
  }
  localStorage.removeItem(SESSION_TOKEN_KEY)
  localStorage.removeItem(usernameKey)
}

/** 修改密码；成功后后端吊销全部会话，需要重新登录。 */
export async function changePassword(oldPassword: string, newPassword: string): Promise<void> {
  await post('/auth/password', { old_password: oldPassword, new_password: newPassword })
}
