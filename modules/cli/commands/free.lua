local cmd = {}

cmd.main = function()
  local total, status = osv_request({"os", "memory", "total"}, "GET")
  osv_resp_assert(status, 200)

  local free, status = osv_request({"os", "memory", "free"}, "GET")
  osv_resp_assert(status, 200)

  table_print({
    {"", "total", "used", "free"},
    {"Mem:", tostring(total), tostring(total - free), tostring(free)}
  })
end

return cmd
