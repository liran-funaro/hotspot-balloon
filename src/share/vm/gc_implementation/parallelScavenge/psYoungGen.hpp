/*
 * Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#ifndef SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSYOUNGGEN_HPP
#define SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSYOUNGGEN_HPP

#include "gc_implementation/parallelScavenge/objectStartArray.hpp"
#include "gc_implementation/parallelScavenge/psGenerationCounters.hpp"
#include "gc_implementation/parallelScavenge/psVirtualspace.hpp"
#include "gc_implementation/shared/mutableSpace.hpp"
#include "gc_implementation/shared/spaceCounters.hpp"

class PSMarkSweepDecorator;

class PSYoungGen : public CHeapObj<mtGC> {
  friend class VMStructs;
  friend class ParallelScavengeHeap;
  friend class AdjoiningGenerations;

 protected:
  MemRegion       _reserved;
  PSVirtualSpace* _virtual_space;

  // Spaces
  MutableSpace* _eden_space;
  MutableSpace* _from_space;
  MutableSpace* _to_space;

  // BALLOONING, YI REN
  // desired balloon size.
  size_t _balloon_size;


  // MarkSweep Decorators
  PSMarkSweepDecorator* _eden_mark_sweep;
  PSMarkSweepDecorator* _from_mark_sweep;
  PSMarkSweepDecorator* _to_mark_sweep;

  // Sizing information, in bytes, set in constructor
  const size_t _init_gen_size;
  const size_t _min_gen_size;
  const size_t _max_gen_size;

  // Performance counters
  PSGenerationCounters*     _gen_counters;
  SpaceCounters*            _eden_counters;
  SpaceCounters*            _from_counters;
  SpaceCounters*            _to_counters;

  // Initialize the space boundaries
  void compute_initial_space_boundaries();

  // Space boundary helper
  void set_space_boundaries(size_t eden_size, size_t survivor_size);

  virtual bool resize_generation(size_t eden_size, size_t survivor_size);
  virtual void resize_spaces(size_t eden_size, size_t survivor_size);

  // Adjust the spaces to be consistent with the virtual space.
  void post_resize();

  // Return number of bytes that the generation can change.
  // These should not be used by PSYoungGen
  virtual size_t available_for_expansion();
  virtual size_t available_for_contraction();

  // Given a desired shrinkage in the size of the young generation,
  // return the actual size available for shrinkage.
  // BALLOONING, YI REN
  // added const qualifier (otherwise compilation fails)
  virtual size_t limit_gen_shrink(size_t desired_change) const;
  // returns the number of bytes available from the current size
  // down to the minimum generation size.
  // BALLOONING, YI REN
  // added const qualifier (otherwise compilation fails)
  size_t available_to_min_gen() const;
  // Return the number of bytes available for shrinkage considering
  // the location the live data in the generation.
  // BALLOONING, YI REN
  // added const qualifier (otherwise compilation fails)
  virtual size_t available_to_live() const;

 public:
  // Initialize the generation.
  PSYoungGen(size_t        initial_byte_size,
             size_t        minimum_byte_size,
             size_t        maximum_byte_size);
  void initialize_work();
  virtual void initialize(ReservedSpace rs, size_t alignment);
  virtual void initialize_virtual_space(ReservedSpace rs, size_t alignment);

  MemRegion reserved() const            { return _reserved; }

  bool is_in(const void* p) const   {
      return _virtual_space->contains((void *)p);
  }

  bool is_in_reserved(const void* p) const   {
      return reserved().contains((void *)p);
  }

  MutableSpace*   eden_space() const    { return _eden_space; }
  MutableSpace*   from_space() const    { return _from_space; }
  MutableSpace*   to_space() const      { return _to_space; }
  PSVirtualSpace* virtual_space() const { return _virtual_space; }

  // For Adaptive size policy
  // BALLOONING, YI REN
  // added const qualifier (otherwise compilation fails)
  size_t min_gen_size() const { return _min_gen_size; }

  // MarkSweep support
  PSMarkSweepDecorator* eden_mark_sweep() const    { return _eden_mark_sweep; }
  PSMarkSweepDecorator* from_mark_sweep() const    { return _from_mark_sweep; }
  PSMarkSweepDecorator* to_mark_sweep() const      { return _to_mark_sweep;   }

  void precompact();
  void adjust_pointers();
  void compact();

  // Called during/after gc
  void swap_spaces();

  // Resize generation using suggested free space size and survivor size
  // NOTE:  "eden_size" and "survivor_size" are suggestions only. Current
  //        heap layout (particularly, live objects in from space) might
  //        not allow us to use these values.
  void resize(size_t eden_size, size_t survivor_size);

  // Size info
  size_t capacity_in_bytes() const;
  size_t used_in_bytes() const;
  size_t free_in_bytes() const;

  size_t capacity_in_words() const;
  size_t used_in_words() const;
  size_t free_in_words() const;

  // The max this generation can grow to
  // BALLOONING, YI REN
  // Substract _balloon_size from virtual_space()->committed_size(),
  // to make adaptive size policy "believe" there is less usable space.
  // As a result it will shrink the generation and we get the ballooning effect.
  // The alignment is important here (wihtout which SIGSEGV may be preoduced)
  // since the return value is assumed to be aligned elsewhere.
  size_t max_size() const {
      // BALLOONING, YI REN
	  // limit_gen_shrink is used by the resizing functions to
	  // avoid over-shrinking, so I should use it as well.
	  // Furthermore, if this limit is not forced, the sanity check in
	  // resize_generation will fail.
	  // we could call limit_gen_shrink(_balloon_size - uncommitted), but the
	  // result will be the same due to the MAX2 later.
  	  size_t lower_bound = virtual_space()->committed_size() - limit_gen_shrink(_balloon_size);
  	  size_t ballooned_size = MAX2(_reserved.byte_size() - _balloon_size, lower_bound);
      // BALLOONING, YI REN
  	  // alignment
  	  return align_size_up(ballooned_size, virtual_space()->alignment());
  }

  // The max this generation can grow to if the boundary between
  // the generations are allowed to move.
  // BALLOONING, YI REN
  // Substract _balloon_size from virtual_space()->committed_size(),
  // to make adaptive size policy "believe" there is less usable space.
  // As a result it will shrink the generation and we get the ballooning effect.
  // The alignment is important here (wihtout which SIGSEGV may be preoduced)
  // since the return value is assumed to be aligned elsewhere.
  size_t gen_size_limit() const {
      // BALLOONING, YI REN
	  // limit_gen_shrink is used by the resizing functions to
	  // avoid over-shrinking, so I should use it as well.
	  // Furthermore, if this limit is not forced, the sanity check in
	  // resize_generation will fail.
	  // we could call limit_gen_shrink(_balloon_size - uncommitted), but the
	  // result will be the same due to the MAX2 later.
  	  size_t lower_bound = virtual_space()->committed_size() - limit_gen_shrink(_balloon_size);
  	  size_t ballooned_size = MAX2(_max_gen_size - _balloon_size, lower_bound);
      // BALLOONING, YI REN
  	  // alignment
  	  return align_size_up(ballooned_size, virtual_space()->alignment());
  }

  bool is_maximal_no_gc() const {
    return true;  // never expands except at a GC
  }

  // Allocation
  HeapWord* allocate(size_t word_size) {
    HeapWord* result = eden_space()->cas_allocate(word_size);
    return result;
  }

  HeapWord** top_addr() const   { return eden_space()->top_addr(); }
  HeapWord** end_addr() const   { return eden_space()->end_addr(); }

  // Iteration.
  void oop_iterate(OopClosure* cl);
  void object_iterate(ObjectClosure* cl);

  virtual void reset_after_change();
  virtual void reset_survivors_after_shrink();

  // Performance Counter support
  void update_counters();

  // Debugging - do not use for time critical operations
  void print() const;
  void print_on(outputStream* st) const;
  void print_used_change(size_t prev_used) const;
  virtual const char* name() const { return "PSYoungGen"; }

  void verify();

  // Space boundary invariant checker
  void space_invariants() PRODUCT_RETURN;

  // Helper for mangling survivor spaces.
  void mangle_survivors(MutableSpace* s1,
                        MemRegion s1MR,
                        MutableSpace* s2,
                        MemRegion s2MR) PRODUCT_RETURN;

  void record_spaces_top() PRODUCT_RETURN;

  // BALLOONING, YI REN
  // get _balloon_size
  size_t balloon_size() const { return _balloon_size; };
  // set _balloon_size to 0
  void init_balloon() {set_balloon_size(0); };
  // set _balloon_size to bytes
  void set_balloon_size(size_t bytes) {
	// _balloon_size can not be greater than max_gen_size
  	size_t new_size = MIN2(bytes, _max_gen_size);
  	// alignment
  	new_size = align_size_down(new_size, virtual_space()->alignment());
  	_balloon_size = new_size;
//  	printf("set young balloon size in bytes:%zu\n", _balloon_size);
  }
};

#endif // SHARE_VM_GC_IMPLEMENTATION_PARALLELSCAVENGE_PSYOUNGGEN_HPP
