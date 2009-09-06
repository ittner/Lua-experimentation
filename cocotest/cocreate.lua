-- The Great Computer Language Shootout
-- http://shootout.alioth.debian.org/
-- contributed by Mike Pall
-- arg[1] is the number of coroutines to create (default 1000).

local co = coroutine
local create, resume, yield = co.create, co.resume, co.yield

local function link(n)
  if n > 1 then
    local _, message = resume(create(link), n-1)
    yield(message + 1)
  else
    yield(1)
  end
end

local N = tonumber(arg and arg[1]) or 1000
local _, message = resume(create(link), N)
io.write(message, " coroutines created\n")
