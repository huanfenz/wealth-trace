import axios, { type AxiosInstance } from 'axios'

interface Envelope<T> {
  code: number
  message: string
  data: T
}

const http: AxiosInstance = axios.create({
  baseURL: '/api',
  timeout: 15000,
})

http.interceptors.response.use(
  (response) => response,
  (error) => {
    const message =
      error?.response?.data?.message ?? error?.message ?? '网络请求失败'
    return Promise.reject(new Error(message))
  },
)

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

export function get<T>(url: string, params?: Record<string, unknown>): Promise<T> {
  return unwrap<T>(http.get(url, { params }))
}

export function post<T>(url: string, body?: unknown): Promise<T> {
  return unwrap<T>(http.post(url, body))
}

export function put<T>(url: string, body?: unknown): Promise<T> {
  return unwrap<T>(http.put(url, body))
}

export default http
