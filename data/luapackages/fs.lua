local fs = {}

fs.move = _fs.move
fs.remove = _fs.remove
fs.copy = _fs.copy
fs.mkdir = _fs.mkdir
fs.list_dir = _fs.list_dir
fs.stat = _fs.stat
fs.stat_fs = _fs.stat_fs
fs.dir_info = _fs.dir_info
fs.ask_select_file = _fs.ask_select_file
fs.ask_select_dir = _fs.ask_select_dir
fs.find = _fs.find
fs.find_all = _fs.find_all
fs.find_not = _fs.find_not
fs.exists = _fs.exists
fs.is_dir = _fs.is_dir
fs.is_file = _fs.is_file
fs.read_file = _fs.read_file
fs.fill_file = _fs.fill_file
fs.make_dummy_file = _fs.make_dummy_file
fs.truncate = _fs.truncate
fs.img_mount = _fs.img_mount
fs.img_umount = _fs.img_umount
fs.get_img_mount = _fs.get_img_mount
fs.hash_file = _fs.hash_file
fs.hash_data = _fs.hash_data
fs.verify = _fs.verify
fs.allow = _fs.allow
fs.sd_is_mounted = _fs.sd_is_mounted
fs.sd_switch = _fs.sd_switch
fs.fix_cmacs = _fs.fix_cmacs
fs.key_dump = _fs.key_dump
fs.cart_dump = _fs.cart_dump

-- compatibility
function os.remove(path)
    local success, allowed, stat, error
    success, stat = pcall(fs.stat, path)
    if not success then
        return nil, path..": No such file or directory", 2
    end
    if stat.type == "dir" then
        -- os.remove can remove an empty directory, so we gotta check
        success, dir_list = pcall(fs.list_dir, path)
        if not success then
            return nil, "Error occurred listing directory: "..dir_list, 2001
        end
        if #dir_list ~= 0 then
            return nil, path..": Directory not empty", 39
        end
    end
    allowed = fs.allow(path)
    if not allowed then
        return nil, path..": Operation not permitted", 1
    end
    success, error = pcall(fs.remove, path, {recursive=true})
    if success then
        return true
    else
        return nil, "Error occurred removing item: "..error, 2001
    end
end

-- compatibility
function os.rename(src, dst)
    error("os.rename is not implemented yet (try fs.move instead)")
    --local success, drv_src, drv_dst, allowed
    --drv_src = string.upper(string.sub(src, 1, 1))
    --drv_dst = string.upper(string.sub(dst, 1, 1))
    --if drv_src ~= drv_dst then
    --    return nil, "Invalid cross-device link", 18
    --end
    --allowed = fs.allow(src) and fs.allow(dst)
    --if not allowed then
    --    return nil, "Permission denied", 13
    --end
end

function fs.write_file(path, offset, data)
    local success, filestat
    if type(offset) == "string" then
        if offset == "end" then
            success, filestat = pcall(fs.stat, path)
            if success then
                offset = filestat.size
            else
                -- allows for this to work on files that don't yet exist
                offset = 0
            end
        else
            error("offset should be an integer or \"end\"")
        end
    elseif type(offset) ~= "number" then
        error("offset should be an integer or \"end\"")
    end

    return _fs.write_file(path, offset, data)
end

function fs.verify_with_sha_file(path)
    local success, file_hash, sha_data, path_sha
    path_sha = path..".sha"
    success, sha_data = pcall(fs.read_file, path_sha, 0, 0x20)
    if not success then
        return nil
    end
    file_hash = fs.hash_file(path, 0, 0)
    return file_hash == sha_data
end

return fs
