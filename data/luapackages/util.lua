local util = {}

-- https://stackoverflow.com/a/71896879
function util.bytes_to_hex(data)
    local hex = {}
    local char
    for i = 1, #data do
        char = string.sub(data, i, i)
        --hex = hex..string.format("%02x", string.byte(char))
        table.insert(hex, string.format("%02x", string.byte(char)))
    end
    return table.concat(hex)
end

-- https://stackoverflow.com/a/9140231
function util.hex_to_bytes(hexstring)
    return (string.gsub(hexstring, '..', function (cc)
        return string.char(tonumber(cc, 16))
    end))
end

-- returns something like "241202"
function util.get_datestamp()
    return os.date("%y%m%d")
end

-- returns something like "010828"
function util.get_timestamp()
    return os.date("%H%M%S")
end

-- https://stackoverflow.com/a/49376823
function util.running_as_module()
    local success, _, __, required = pcall(debug.getlocal, 4, 1)
    if not success then
        -- umm uhh
        return false
    end
    -- for the file being executed directly, this seems to be a number
    -- but for a file being required by another, it returns the string given to require()
    -- example: if foo.lua is executed, this should return a number like 2
    -- but when foo.lua requires bar.lua, bar.lua should get "bar" as the result
    -- however, this behavior seems inconsistent between lua versions
    -- the Stack Overflow answer seems to suggest the call would fail on some version before 5.4
    -- honestly, I would've liked a better method, but i have yet to find some way to get the executing file
    -- in the C code in LoadLuaFile, I could set the string that contains the full filepath, but... where do I set it?
    return type(required) == "string"
end

return util
