local cmd = {}

cmd.main = function()
    local content, status = osv_request({"os", "dmesg"}, "GET")
    osv_resp_assert(status, 200)
    print(content)
end

return cmd
