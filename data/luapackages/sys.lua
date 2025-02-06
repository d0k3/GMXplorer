local sys = {}

sys.boot = _sys.boot
sys.reboot = _sys.reboot
sys.power_off = _sys.power_off
sys.check_embedded_backup = _sys.check_embedded_backup
sys.check_raw_rtc = _sys.check_raw_rtc

sys.secureinfo_letter = nil
sys.region = nil
sys.serial = nil
sys.sys_id0 = nil
sys.emu_id0 = nil
sys.emu_base = nil

local regions = {"JPN", "USA", "EUR", "AUS", "CHN", "KOR", "TWN"}

local function refresh_secureinfo()
    local letter = nil
    if fs.exists("1:/rw/sys/SecureInfo_A") then letter = "A"
    elseif fs.exists("1:rw/sys/SecureInfo_B") then letter = "B"
    else error("could not read SecureInfo") end

    local secinfo = fs.read_file("1:/rw/sys/SecureInfo_"..letter, 0, 0x111)

    -- remember, Lua starts indexes at 1 so these offsets appear off by one if you're used to other langs
    local serial = string.sub(secinfo, 0x103, 0x111)
    serial = string.gsub(serial, "\0", "")

    local region_byte = string.sub(secinfo, 0x101, 0x101)
    local region_num = string.byte(region_byte)
    if region_num > 6 then error("SecureInfo region byte is invalid") end

    sys.serial = serial
    sys.region = regions[region_num + 1]
    sys.secureinfo_letter = letter
end

local function refresh_id0()
    local sys_id0 = _sys.get_id0("1:/private/movable.sed")

    if fs.exists("4:/private/movable.sed") then
        local emu_id0 = _sys.get_id0("4:/private/movable.sed")
    end

    sys.sys_id0 = sys_id0
    sys.emu_id0 = emu_id0
end

function sys.refresh_info()
    refresh_secureinfo()
    refresh_id0()
    sys.emu_base = _sys.get_emu_base()
end

function sys.next_emu()
    _sys.next_emu()
    pcall(sys.refresh_info())
end

-- in the preload scripts, we have to avoid possibilities of unhandled exceptions
pcall(sys.refresh_info)

return sys
