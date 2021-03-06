// -*- mode: c++; coding: utf-8 -*-
/// @file ply.hh
/// @brief Traverse (ply) array or array expression or array statement.

// (c) Daniel Llorens - 2013-2019
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

// TODO Lots of room for improvement: small (fixed sizes) and large (tiling, etc. see eval.cc in Blitz++).

#pragma once
#include "ra/atom.hh"
#include <functional>

namespace ra {


// --------------
// Run time order
// --------------

// Traverse array expression looking to ravel the inner loop.
// size(k) has a single value.
// adv(k), stride(k), keep_stride(st, k, l) and flat() are used on all the leaf arguments.
// The strides must give 0 for k>=their own rank, to allow frame matching.
// TODO Traversal order should be a parameter, since some operations (e.g. output, ravel) require a specific order.
template <RaIterator A>
inline void
ply_ravel(A && a)
{
    rank_t rank = a.rank();
    assert(rank>=0); // FIXME see test in [ra40].
    rank_t order[rank];
    for (rank_t i=0; i<rank; ++i) {
        order[i] = rank-1-i;
    }
    switch (rank) {
    case 0: *(a.flat()); return;
    case 1: break;
    default: // TODO find a decent heuristic
        // if (rank>1) {
        //     std::sort(order, order+rank, [&a, &order](auto && i, auto && j)
        //               { return a.size(order[i])<a.size(order[j]); });
        // }
        ;
    }
// outermost compact dim.
    rank_t * ocd = order;
    auto ss = a.size(*ocd);
    for (--rank, ++ocd; rank>0 && a.keep_stride(ss, order[0], *ocd); --rank, ++ocd) {
        ss *= a.size(*ocd);
    }
    dim_t sha[rank], ind[rank];
    for (int k=0; k<rank; ++k) {
        ind[k] = 0;
        sha[k] = a.size(ocd[k]);
        if (sha[k]==0) { // for the ravelled dimensions ss takes care.
            return;
        }
        RA_CHECK(sha[k]!=DIM_BAD, "undefined dim ", ocd[k]);
    }
// all sub xpr strides advance in compact dims, as they might be different.
    auto const ss0 = a.stride(order[0]);
// TODO Blitz++ uses explicit stack of end-of-dim p positions, has special cases for common/unit stride.
    for (;;) {
        dim_t s = ss;
        for (auto p=a.flat(); s>0; --s, p+=ss0) {
            *p;
        }
        for (int k=0; ; ++k) {
            if (k>=rank) {
                return;
            } else if (ind[k]<sha[k]-1) {
                ++ind[k];
                a.adv(ocd[k], 1);
                break;
            } else {
                ind[k] = 0;
                a.adv(ocd[k], 1-sha[k]);
            }
        }
    }
}


// -------------------------
// Compile time order.
// -------------------------

#ifdef RA_INLINE
#error bad definition
#endif
#define RA_INLINE inline /* __attribute__((always_inline)) inline */

template <class order, int ravel_rank, class A, class S>
RA_INLINE constexpr void
subindex(A & a, dim_t s, S const & ss0)
{
    if constexpr (mp::len<order> == ravel_rank) {
        for (auto p=a.flat(); s>0; --s, p+=ss0) {
            *p;
        }
    } else if constexpr (mp::len<order> > ravel_rank) {
        dim_t size = a.size(mp::first<order>::value); // TODO Precompute these at the top
        for (dim_t i=0, iend=size; i<iend; ++i) {
            subindex<mp::drop1<order>, ravel_rank>(a, s, ss0);
            a.adv(mp::first<order>::value, 1);
        }
        a.adv(mp::first<order>::value, -size);
    } else {
        abort();
    }
}

// until() converts runtime jj into compile time j. TODO a.adv<k>().
template <class order, int j, class A, class S>
RA_INLINE constexpr void
until(int const jj, A & a, dim_t const s, S const & ss0)
{
    if constexpr (mp::len<order> < j) {
        assert(0 && "rank too high");
    } else if constexpr (mp::len<order> >= j) {
        if (jj==j) {
            subindex<order, j>(a, s, ss0);
        } else {
            until<order, j+1>(jj, a, s, ss0);
        }
    } else {
        abort();
    }
}

// find the outermost compact dim.
template <class A>
constexpr auto
ocd(A && a)
{
    rank_t const rank = a.rank();
    auto s = a.size(rank-1);
    int j = 1;
    while (j<rank && a.keep_stride(s, rank-1, rank-1-j)) {
        s *= a.size(rank-1-j);
        ++j;
    }
    return std::make_tuple(s, j);
};

template <RaIterator A>
RA_INLINE constexpr void
plyf(A && a)
{
    constexpr rank_t rank = rank_s<A>();
    static_assert(rank>=0, "plyf needs static rank");

    if constexpr (rank_s<A>()==0) {
        *(a.flat());
    } else if constexpr (rank_s<A>()==1) {
        subindex<mp::iota<1>, 1>(a, a.size(0), a.stride(0));
    } else if constexpr (0 && size_s<A>()>=0) {
// this can only be enabled when f() will be constexpr; size_s isn't enough b/c of keep_stride.
// test/concrete.cc has a case that shows this.
// cf https://stackoverflow.com/questions/55288555
        constexpr auto sj = ocd(a);
        constexpr auto s = std::get<0>(sj);
        constexpr auto j = std::get<1>(sj);
// all sub xpr strides advance in compact dims, as they might be different.
// send with static j. Note that order here is inverse of order.
        until<mp::iota<rank_s<A>()>, 0>(j, a, s, a.stride(rank-1));
    } else {
// the unrolling above isn't worth it when s, j cannot be constexpr.
        auto s = a.size(rank-1);
        subindex<mp::iota<rank_s<A>()>, 1>(a, s, a.stride(rank-1));
    }
}

#undef RA_INLINE


// ---------------------------
// Select best performance (or requirements) for each type.
// ---------------------------

template <RaIterator A>
inline constexpr void
ply(A && a)
{
    if constexpr (size_s<A>()==DIM_ANY) {
        ply_ravel(std::forward<A>(a));
    } else {
        plyf(std::forward<A>(a));
    }
}


// ---------------------------
// Short-circuiting pliers.
// ---------------------------

// TODO Refactor with ply_ravel. Make exit available to plyf.
// TODO These are reductions. How to do higher rank?
template <RaIterator A, class DEF>
inline auto
ply_ravel_exit(A && a, DEF && def)
{
    rank_t rank = a.rank();
    assert(rank>=0); // FIXME see test in [ra40].
    rank_t order[rank];
    for (rank_t i=0; i<rank; ++i) {
        order[i] = rank-1-i;
    }
    switch (rank) {
    case 0: {
        if (auto what = *(a.flat()); std::get<0>(what)) {
            return std::get<1>(what);
        }
        return def;
    }
    case 1: break;
    default: // TODO find a decent heuristic
        // if (rank>1) {
        //     std::sort(order, order+rank, [&a, &order](auto && i, auto && j)
        //               { return a.size(order[i])<a.size(order[j]); });
        // }
        ;
    }
// outermost compact dim.
    rank_t * ocd = order;
    auto ss = a.size(*ocd);
    for (--rank, ++ocd; rank>0 && a.keep_stride(ss, order[0], *ocd); --rank, ++ocd) {
        ss *= a.size(*ocd);
    }
    dim_t sha[rank], ind[rank];
    for (int k=0; k<rank; ++k) {
        ind[k] = 0;
        sha[k] = a.size(ocd[k]);
        if (sha[k]==0) { // for the ravelled dimensions ss takes care.
            return def;
        }
        RA_CHECK(sha[k]!=DIM_BAD, "undefined dim ", ocd[k]);
    }
// all sub xpr strides advance in compact dims, as they might be different.
    auto const ss0 = a.stride(order[0]);
// TODO Blitz++ uses explicit stack of end-of-dim p positions, has special cases for common/unit stride.
    for (;;) {
        dim_t s = ss;
        for (auto p=a.flat(); s>0; --s, p+=ss0) {
            if (auto what = *p; std::get<0>(what)) {
                return std::get<1>(what);
            }
        }
        for (int k=0; ; ++k) {
            if (k>=rank) {
                return def;
            } else if (ind[k]<sha[k]-1) {
                ++ind[k];
                a.adv(ocd[k], 1);
                break;
            } else {
                ind[k] = 0;
                a.adv(ocd[k], 1-sha[k]);
            }
        }
    }
}

template <RaIterator A, class DEF>
inline decltype(auto)
early(A && a, DEF && def)
{
    return ply_ravel_exit(std::forward<A>(a), std::forward<DEF>(def));
}

} // namespace ra
