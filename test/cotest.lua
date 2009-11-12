-- Simple test for resume/yield semantics across C call boundaries.
-- Note: this works across metamethods, iterators, etc., too.

local resume, yield = coroutine.resume, coroutine.yield

local function verify(what, expect, ...)
  local got = {...}
  for i=1,100 do
    if expect[i] ~= got[i] then
      print("FAIL", what)
      return
    end
    if expect[i] == nil then
      print("OK  ", what)
      break
    end
  end
end

local function cofunc(...)
  verify("call", { 1, "foo" }, ...)
  verify("yield", { "bar" }, yield(2, "test"))
  verify("pcall yield", { true, "again" }, pcall(yield, "from pcall"))
  return "end"
end

if coroutine.coco then
  print("Coco is available. All tests should work:")
else
  print("Coco is NOT available. The tests should fail starting with pcall yield:")
end

local co = coroutine.create(cofunc)
verify("resume", { true, 2, "test" }, resume(co, 1, "foo"))
verify("resume pcall", { true, "from pcall" }, resume(co, "bar"))
verify("resume end", { true, "end" }, resume(co, "again"))

