# lib Intrinsics
#   fun prefetch = "llvm.prefetch"(address : Void*, rw : Int32, locality : Int32, cache_type : Int32)
# end

# @[AlwaysInline]
# def prefetch(p)
#   Intrinsics.prefetch(p, 0, 1, 1)
# end

# TODO
@[AlwaysInline]
def radixsort_lookahead(p)
  # prefetch(p.unsafe_as(Pointer(UInt8))+16)
end

struct RadixHelper(T, WIDTH, BITS)
  @[AlwaysInline]
  def self.hash_key(src, i, &block)
    {% begin %}
    {% offset = 4 * 8 - WIDTH %}
    {% size = 1u32 << (BITS < WIDTH ? BITS : WIDTH) %}
    {% mask = size - 1 %}
    v = yield(src.unsafe_fetch(i))
    (v.unsafe_shr({{offset}}))&{{mask}}
    {% end %}
  end

  def self.lsd_impl(src : Slice(T), dst : Slice(T), n, &block)
    {% begin %}
    {% offset = 4 * 8 - WIDTH %}
    {% size = 1u32 << (BITS < WIDTH ? BITS : WIDTH) %}
    {% mask = size - 1 %}
    c = StaticArray(UInt32, {{2 * size}}).new(0)
    # Cumulative distribution function. Unrolled x2 to mitigate store->load hit.
    (n >> 1).times do |i|
      k0 = hash_key(src, 2*i){|x| yield x}
      k1 = hash_key(src, 2*i+1){|x| yield x}
      c.to_unsafe[2*k0] += 1
      c.to_unsafe[2*k1+1] += 1
    end
    if n.odd?
      kn = hash_key src, n-1 {|x| yield x} 
      c.to_unsafe[2*kn] += 1
    end
    s = 0_u32
    t = 0_u32
    {{size}}.times do |i|
      t = s
      s += c.unsafe_fetch(2*i)+c.unsafe_fetch(2*i+1)
      c.to_unsafe[i] = t
    end
    skip = false
    {{size - 1}}.times do |i|
      if c.unsafe_fetch(i+1)-c.unsafe_fetch(i)==n # All keys are in the same bucket.
        src, dst = dst, src
        skip = true
        break
      end
    end
    # Scatter.
    unless skip
      n.times do |i|
        k = hash_key(src, i){|x| yield x}
        # radixsort_lookahead(dst+c[k], (n-c[k])*sizeof(T))
        # radixsort_lookahead(dst+c[k])
        # prefetch(dst.to_unsafe+c[k]+4)
        dst.to_unsafe[c.unsafe_fetch(k)] = src.unsafe_fetch(i)
        c.to_unsafe[k]+=1
      end
    end
    {% if BITS < WIDTH %}
      return RadixHelper(T, {{ WIDTH - BITS }},BITS).lsd_impl(dst, src, n){|x| yield x}
    {% else %}
      return dst
    {% end %}
    
  {% end %}
  end

  def self.do_lsd(src, tmp = nil, &block)
    n = src.size
    src = src.to_unsafe.to_slice(n)
    if tmp
      raise "buffer is too small" if tmp.size < n
      tmp = tmp.to_unsafe.to_slice(n)
    else
      # tmp = src.dup
      tmp = Slice(T).new(n, src[0])
    end
    ret = lsd_impl(src, tmp, n) { |x| yield(x) }
    src.copy_from(ret) if ret.to_unsafe != src.to_unsafe
    src
  end
end

module Enumerable(T)
  def radix_sort_by!(*, tmp : Enumerable(T)? = nil, &block : T -> U) forall U
    {% begin %}
    {% if U == UInt32 %}
      RadixHelper(T, 32, 8).do_lsd(self, tmp) {|x| yield(x)}
    {% elsif U == Int32 %}
      RadixHelper(T, 32, 8).do_lsd(self, tmp) {|x| yield(x).unsafe_as(UInt32) &- Int32::MIN.unsafe_as(UInt32)}
    {% else %}
      {% raise "Block should return UInt32 (preferably) or Int32, instead of #{U}" %}
    {% end %}
    {% end %}
  end
end
