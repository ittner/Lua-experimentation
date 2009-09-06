-- The Great Computer Language Shootout
-- http://shootout.alioth.debian.org/
-- contributed by Mike Pall
-- arg[1] is the number of runs (default 10000).
-- arg[2] is the number of coroutines to create (default 100).

local co = coroutine
local wrap, yield = co.wrap, co.yield
if co.cstacksize then co.cstacksize(1) end -- Use minimum C stack.

local function link(n)
  if n > 1 then
    local cofunc = wrap(link)
    cofunc(n-1)
    yield()
    repeat yield(cofunc() + 1) until false
  else
    repeat yield(1) until false
  end
end

local N = tonumber(arg and arg[1]) or 10000
local M = tonumber(arg and arg[2]) or 100
local cofunc = wrap(link)
cofunc(M)
local count = 0
for i = 1,N do count = count + cofunc() end
io.write(count, "\n")

