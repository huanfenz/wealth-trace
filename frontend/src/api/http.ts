// Axios 封装：统一基础路径 /api、超时时间，并把后端的统一响应包解包为业务数据。
import axios, { type AxiosInstance } from 'axios'

/** 后端统一响应包结构：code 为 0 表示成功，data 为业务数据。 */
interface Envelope<T> {
  code: number
  message: string
  data: T
}

const http: AxiosInstance = axios.create({
  baseURL: '/api',
  timeout: 15000,
})

// HTTP 状态码兜底文案：后端未返回 message 时用于给出人性化提示。
const statusMessages: Record<number, string> = {
  400: '请求参数有误，请检查后重试',
  401: '登录状态已失效，请重新登录',
  403: '没有权限执行该操作',
  404: '请求的资源不存在',
  405: '当前操作不被服务器支持，请确认后端已更新并重启',
  409: '操作与当前数据状态冲突，无法完成',
  500: '服务器内部错误，请稍后重试',
  503: '服务暂时不可用，请稍后重试',
}

// 响应错误拦截：优先取后端返回的 message，其次按 HTTP 状态码给出友好提示，
// 再退回底层错误信息，统一转换为 Error 抛出。
http.interceptors.response.use(
  (response) => response,
  (error) => {
    const data = error?.response?.data
    const status = error?.response?.status as number | undefined
    const backendMessage = typeof data === 'string' ? data : data?.message
    const message =
      (backendMessage && String(backendMessage).trim()) ||
      (status ? statusMessages[status] : undefined) ||
      error?.message ||
      '网络请求失败'
    return Promise.reject(new Error(message))
  },
)

/** 校验统一响应包：code !== 0 或格式异常时抛错，成功则返回其中的 data。 */
async function unwrap<T>(promise: Promise<{ data: Envelope<T> }>): Promise<T> {
  const response = await promise
  const envelope = response.data
  if (envelope && typeof envelope === 'object' && 'code' in envelope) {
    if (envelope.code !== 0) {
      throw new Error(envelope.message || '请求失败')
    }
    return envelope.data
  }
  throw new Error('响应格式不正确')
}

/** 发起 GET 请求并返回解包后的业务数据。 */
export function get<T>(url: string, params?: Record<string, unknown>): Promise<T> {
  return unwrap<T>(http.get(url, { params }))
}

/** 发起 POST 请求并返回解包后的业务数据。 */
export function post<T>(url: string, body?: unknown): Promise<T> {
  return unwrap<T>(http.post(url, body))
}

/** 发起 PUT 请求并返回解包后的业务数据。 */
export function put<T>(url: string, body?: unknown): Promise<T> {
  return unwrap<T>(http.put(url, body))
}

/** 发起 DELETE 请求并返回解包后的业务数据。 */
export function del<T>(url: string): Promise<T> {
  return unwrap<T>(http.delete(url))
}

export default http
