// -*- mode: c++; coding: utf-8 -*-
/// @file ra-0.cc
/// @brief Checks for ra:: arrays, iterators.

// (c) Daniel Llorens - 2013-2015
// This library is free software; you can redistribute it and/or modify it under
// the terms of the GNU Lesser General Public License as published by the Free
// Software Foundation; either version 3 of the License, or (at your option) any
// later version.

#include <iostream>
#include <iterator>
#include <numeric>
#include "ra/test.hh"
#include "ra/ra.hh"
#include "ra/mpdebug.hh"

using std::cout, std::endl, std::flush, ra::TestRecorder;
template <int i> using TI = ra::TensorIndex<i, int>;

template <class A>
void CheckArrayOutput(TestRecorder & tr, A const & a, double * begin)
{
    std::ostringstream o;
    o << a;
    cout << "a: " << o.str() << endl;
    std::istringstream i(o.str());
    std::istream_iterator<double> iend;
    std::istream_iterator<double> ibegin(i);
    tr.test(std::equal(ibegin, iend, begin));
}

template <class A>
void CheckArrayIO(TestRecorder & tr, A const & a, double * begin)
{
    std::ostringstream o;
    o << a;
    {
        std::istringstream i(o.str());
        std::istream_iterator<double> iend;
        std::istream_iterator<double> ibegin(i);
        tr.info("reading back from '", o.str(), "'").test(std::equal(ibegin, iend, begin));
    }
    {
        std::istringstream i(o.str());
        A b;
        tr.test(bool(i));
        i >> b;
        auto as = ra::shape(a);
        auto bs = ra::shape(b);
        tr.info("shape from '", o.str(), "'").test(std::equal(as.begin(), as.end(), bs.begin()));
        tr.info("content").test(std::equal(a.begin(), a.begin(), b.begin()));
    }
}

int main()
{
    TestRecorder tr(std::cout);
    tr.section("concepts");
    {
        ra::Small<int, 2> a(0.), b(0.);
        auto e0 = ra::expr([](int a, int b) { return a+b; }, start(a), start(b));
        static_assert(std::is_literal_type_v<decltype(e0)>);
        auto e1 = ra::expr([](int a, int b) { return a+b; }, start(a), ra::scalar(0.));
        static_assert(std::is_literal_type_v<decltype(e1)>);
        auto e2 = ra::expr([](int a, int b) { return a+b; }, start(a), ra::iota(2));
        static_assert(std::is_literal_type_v<decltype(e2)>);
    }
    tr.section("internal fields");
    {
        {
            double aa[10];
            aa[0] = 99;
            ra::View<double, 1> a { {{10, 1}}, aa }; // [ra36]
            tr.test_eq(99., a.p[0]);
        }
        {
            double aa[6] = { 1, 2, 3, 4, 5, 6 };
            aa[0] = 99;
            ra::View<double, 2> a { { {3, 2}, {2, 1}}, aa };
            tr.test_eq(4., a(1, 1));
            tr.test_eq(99., a.p[0]);
        }
        {
            double aa[20];
            aa[19] = 77;
            ra::View<double> a = { {{10, 2}, {2, 1}}, aa };
            tr.test_eq(10, a.dim[0].size);
            tr.test_eq(2, a.dim[1].size);
            cout << "a.p(3, 4): " << a.p[19] << endl;
            tr.test_eq(77, a.p[19]);
        }
        {
            auto pp = std::unique_ptr<double []>(new double[10]);
            pp[9] = 77;
            double * p = pp.get();
            ra::Unique<double> a {};
            a.store = std::move(pp);
            a.p = p;
            a.dim = {{5, 2}, {2, 1}};
            tr.test_eq(5, a.dim[0].size);
            tr.test_eq(2, a.dim[1].size);
            cout << "a.p(3, 4): " << a.p[9] << endl;
            tr.test_eq(77, a.p[9]);
        }
        {
            auto pp = std::shared_ptr<double>(new double[10]);
            pp.get()[9] = 88;
            double * p = pp.get();
            ra::Shared<double> a {};
            a.store = pp;
            a.p = p;
            a.dim = {{5, 2}, {2, 1}};
            tr.test_eq(5, a.dim[0].size);
            tr.test_eq(2, a.dim[1].size);
            cout << "a.p(3, 4): " << a.p[9] << endl;
            tr.test_eq(88, a.p[9]);
        }
        {
            decltype(std::declval<ra::Big<double>>().store) pp(10);
            pp[9] = 99;
            double * p = pp.data();
            ra::Big<double> a {};
            a.store = pp;
            a.p = p;
            a.dim = {{5, 2}, {2, 1}};
            tr.test_eq(5, a.dim[0].size);
            tr.test_eq(2, a.dim[1].size);
            cout << "a.p(3, 4): " << a.p[9] << endl;
            tr.test_eq(99, a.p[9]);
        }
    }
    tr.section("rank 0 -> scalar with Small");
    {
        auto rank0test0 = [](double & a) { a *= 2; };
        auto rank0test1 = [](double const & a) { return a*2; };
        ra::Small<double> a { 33 };
        static_assert(sizeof(a)==sizeof(double), "bad assumption");
        rank0test0(a);
        tr.test_eq(66, a);
        double b = rank0test1(a);
        tr.test_eq(66, a);
        tr.test_eq(132, b);
    }
    tr.section("(170) rank 0 -> scalar with View");
    {
        auto rank0test0 = [](double & a) { a *= 2; };
        auto rank0test1 = [](double const & a) { return a*2; };
        double x { 99 };
        ra::View<double, 0> a { {}, &x };
        tr.test_eq(1, a.size());

// ra::View<T, 0> contains a pointer to T plus the dope vector of type Small<Dim, 0>. But after I put the data of Small in Small itself instead of in SmallBase, sizeof(Small<T, 0>) is no longer 0. That was specific of gcc, so better not to depend on it anyway...
        cout << "a()" << a() << endl;
        cout << "sizeof(a())" << sizeof(a()) << endl;
        cout << "sizeof(double *)" << sizeof(double *) << endl;
        // static_assert(sizeof(a())==sizeof(double *), "bad assumption");

        rank0test0(a);
        tr.test_eq(198, a);
        double b = rank0test1(a);
        tr.test_eq(198, a);
        tr.test_eq(396, b);
    }
    tr.section("rank 0 and rank 1 constructors with RANK_ANY");
    {
        ra::Big<int> x {9};
        tr.test_eq(9, x(0));
        tr.test_eq(1, x.size());
        tr.test_eq(1, x.size(0));
        tr.test_eq(1, x.rank());
        ra::Big<int> y = 9;
        tr.test_eq(9, y());
        tr.test_eq(1, y.size());
        tr.test_eq(0, y.rank());
// // FIXME ra::Big<int> x {} is WHO NOSE territory, but should probably be rank 1, size 0
//         ra::Big<int> z {};
//         tr.test_eq(0, z.size());
//         tr.test_eq(1, z.rank());
    }
    tr.section("generic container functions");
    {
        {
            using double2x3 = ra::Small<double, 2, 3>;
            double2x3 r { 1, 2, 3, 4, 5, 6 };
            tr.test_eq(2, rank(r));
// Not sure why Koenig doesn't work here, might be library defect [ra10].
            tr.test_eq(6, ra::size(r));
        }
        {
            double pool[6] = { 1, 2, 3, 4, 5, 6 };
            ra::View<double> r { {{3, 2}, {2, 1}}, pool };
            tr.test_eq(2, rank(r));
            tr.test_eq(6, size(r));
        }
        {
            double pool[6] = { 1, 2, 3, 4, 5, 6 };
            ra::View<double, 2> r {{ra::Dim {3, 2}, ra::Dim {2, 1}}, pool };
            tr.test_eq(2, rank(r));
            tr.test_eq(6, size(r));
        }
    }
    tr.section("iterator for View (I)");
    {
        double chk[6] = { 0, 0, 0, 0, 0, 0 };
        double pool[6] = { 1, 2, 3, 4, 5, 6 };
        ra::View<double> r { {{3, 2}, {2, 1}}, pool };
        ra::cell_iterator<ra::View<double>> it(r.dim, r.p);
        tr.test(r.data()==it.c.p);
        std::copy(r.begin(), r.end(), chk);
        tr.test(std::equal(pool, pool+6, r.begin()));
    }
    tr.section("iterator for View (II)");
    {
        double chk[6] = { 0, 0, 0, 0, 0, 0 };
        double pool[6] = { 1, 2, 3, 4, 5, 6 };
        ra::View<double, 1> r { { ra::Dim {6, 1}}, pool };
        ra::cell_iterator<ra::View<double, 1>> it(r.dim, r.p);
        cout << "View<double, 1> it.c.p: " << it.c.p << endl;
        std::copy(r.begin(), r.end(), chk);
        tr.test(std::equal(pool, pool+6, r.begin()));
    }
    // some of these tests are disabled depending on cell_iterator::operator=.
    tr.section("[ra11a] (skipped) cell_iterator operator= (from cell_iterator) does NOT copy contents");
    {
        double a[6] = { 0, 0, 0, 0, 0, 0 };
        double b[6] = { 1, 2, 3, 4, 5, 6 };
        ra::View<double> ra { {{3, 2}, {2, 1}}, a };
        ra::View<double> rb { {{3, 2}, {2, 1}}, b };
        auto aiter = ra.iter();
        {
            auto biter = rb.iter();
            aiter = biter;
            tr.skip().test_eq(0, ra);
            tr.skip().test_eq(rb, aiter);
        }
        {
            aiter = rb.iter();
            tr.skip().test_eq(0, ra);
            tr.skip().test_eq(rb, aiter);
        }
    }
    tr.section("[ra11b] cell_iterator operator= (from cell_iterator) DOES copy contents");
    {
        ra::Unique<double, 2> A({6, 7}, ra::_0 - ra::_1);
        ra::Unique<double, 2> AA({6, 7}, 0.);
        AA.iter<1>() = A.iter<1>();
        tr.test_eq(ra::_0 - ra::_1, AA);
        tr.test_eq(A, AA);
    }
    tr.section("[ra11b] cell_iterator operator= (from cell_iterator) DOES copy contents");
    {
        ra::Small<double, 6, 7> A = ra::_0 - ra::_1;
        ra::Small<double, 6, 7> AA = 0.;
        AA.iter<1>() = A.iter<1>();
        tr.test_eq(ra::_0 - ra::_1, AA);
        tr.test_eq(A, AA);
    }
    tr.section("[ra11c] STL-type iterators never copy contents");
    {
        double a[6] = { 0, 0, 0, 0, 0, 0 };
        double b[6] = { 1, 2, 3, 4, 5, 6 };
        ra::View<double> ra { {{3, 2}, {2, 1}}, a };
        ra::View<double> rb { {{3, 2}, {2, 1}}, b };
        auto aiter = ra.begin();
        {
            auto biter = rb.begin();
            aiter = biter;
            tr.test_eq(0, ra); // ra unchanged
            tr.test(std::equal(rb.begin(), rb.end(), aiter)); // aiter changed
        }
        {
            aiter = rb.begin();
            tr.test_eq(0, ra); // ra unchanged
            tr.test(std::equal(rb.begin(), rb.end(), aiter)); // aiter changed
        }
    }
    tr.section("shape of .iter()");
    {
        auto test = [&tr](auto && A)
            {
                tr.test_eq(ra::Small<ra::dim_t, 2> {6, 7}, ra::shape(A));
                tr.test_eq(ra::Small<ra::dim_t, 2> {6, 7}, shape(A.iter()));
                tr.test_eq(ra::Small<ra::dim_t, 2> {6, 7}, shape(iter<0>(A)));
                tr.test_eq(ra::Small<ra::dim_t, 2> {6, 7}, shape(iter<-2>(A)));
                tr.test_eq(ra::Small<ra::dim_t, 1> {6}, shape(iter<1>(A)));
                tr.test_eq(ra::Small<ra::dim_t, 1> {6}, shape(iter<-1>(A)));
                tr.test_eq(ra::Small<ra::dim_t, 0> {}, shape(iter<2>(A)));
            };
        test(ra::Unique<double, 2>({6, 7}, ra::_0 - ra::_1));
        test(ra::Unique<double>({6, 7}, ra::_0 - ra::_1));
    }
    tr.section("STL-type iterators");
    {
        double rpool[6] = { 1, 2, 3, 4, 5, 6 };
        ra::View<double, 1> r { {ra::Dim {6, 1}}, rpool };

        double spool[6] = { 0, 0, 0, 0, 0, 0 };
        ra::View<double> s { {{3, 1}, {2, 3}}, spool };

        std::copy(r.begin(), r.end(), s.begin());

        double cpool[6] = { 1, 3, 5, 2, 4, 6 };
        tr.test(std::equal(cpool, cpool+6, spool));
    }
    tr.section("storage types");
    {
        double pool[6] = { 1, 2, 3, 4, 5, 6 };

        ra::Shared<double> s({3, 2}, pool, pool+6);
        tr.test_eq(2, s.rank());
        tr.test(std::equal(pool, pool+6, s.begin()));

        ra::Unique<double> u({3, 2}, pool, pool+6);
        tr.test_eq(2, u.rank());
        tr.test(std::equal(pool, pool+6, u.begin()));

        ra::Big<double> o({3, 2}, pool, pool+6);
        tr.test_eq(2, o.rank());
        tr.test(std::equal(pool, pool+6, o.begin()));
    }
    tr.section("copy between arrays, construct from iterator pair");
    {
        // copy from Fortran order to C order.
        double rpool[6] = { 1, 2, 3, 4, 5, 6 };
        double check[6] = { 1, 4, 2, 5, 3, 6 };

        ra::View<double> r { {{3, 1}, {2, 3}}, rpool };
        std::copy(r.begin(), r.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
        tr.test(std::equal(check, check+6, r.begin()));

        ra::Unique<double> u({3, 2}, r.begin(), r.end());
        std::copy(u.begin(), u.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
        tr.test(std::equal(check, check+6, u.begin()));

        // Small strides are tested in test/small-0.cc, test/small-1.cc.
        ra::Small<double, 3, 2> s { 1, 4, 2, 5, 3, 6 };
        std::copy(s.begin(), s.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
        tr.test(std::equal(check, check+6, s.begin()));

        // construct Small from pointers / iterators. These still work by prefix match.
        // If you want row major fill, use std::copy() after construction.
        double rrcheck[6] = { 1, 1, 2, 2, 3, 3 };
        auto check_ptr
            = [&](auto && rr)
              {
                  ra::Small<double, 3, 2> z(ra::ptr(rr));
                  std::copy(z.begin(), z.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
                  tr.test(std::equal(rrcheck, rrcheck+6, z.begin()));
              };
        {
            std::vector<double> rr { 1, 2, 3, 4, 5, 6 };
            check_ptr(rr.begin());
        }
        {
            double rr[6] { 1, 2, 3, 4, 5, 6 };
            check_ptr(rr);
        }
        {
            ra::Unique<double, 1> rr = { 1, 2, 3, 4, 5, 6 };
            check_ptr(rr.begin());
        }
    }
// In this case, the View + shape provides the driver.
    tr.section("construct View from shape + driverless xpr");
    {
        {
            int checkb[6] = { 0, 0, 1, 1, 2, 2 };
            ra::Unique<int, 2> b({3, 2}, ra::_0);
            tr.test(std::equal(checkb, checkb+6, b.begin()));
        }
// This requires the driverless xpr dyn(scalar, tensorindex) to be constructible.
        {
            int checkb[6] = { 3, 3, 4, 4, 5, 5 };
            ra::Unique<int, 2> b({3, 2}, ra::expr([](int a, int b) { return a+b; }, ra::scalar(3), start(ra::_0)));
            tr.test(std::equal(checkb, checkb+6, b.begin()));
        }
        {
            int checkb[6] = { 0, -1, 1, 0, 2, 1 };
            ra::Unique<int, 2> b({3, 2}, ra::expr([](int a, int b) { return a-b; }, start(ra::_0), start(ra::_1)));
            tr.test(std::equal(checkb, checkb+6, b.begin()));
        }
// TODO Check this is an error (chosen driver is TI<2>, that can't drive) [ra42]
        // {
        //     ra::Unique<int, 2> b({3, 2}, ra::expr([](int a, int b) { return a-b; }, ra::TI<2>, ra::TI<1>));
        //     cout << b << endl;
        // }
// TODO Could this be made to bomb at compile time? [ra42]
        // {
        //     ra::Unique<int> b({3, 2}, ra::expr([](int a, int b) { return a-b; }, ra::TI<2>, ra::TI<1>));
        //     cout << b << endl;
        // }
    }
    tr.section("construct View from shape + xpr");
    {
        double checka[6] = { 9, 9, 9, 9, 9, 9 };
        ra::Unique<double, 2> a({3, 2}, ra::scalar(9));
        tr.test(std::equal(checka, checka+6, a.begin()));
        double checkb[6] = { 11, 11, 22, 22, 33, 33 };
        ra::Unique<double, 2> b({3, 2}, ra::Small<double, 3> { 11, 22, 33 });
        tr.test(std::equal(checkb, checkb+6, b.begin()));
    }
    tr.section("construct Unique from Unique");
    {
        double check[6] = { 2, 3, 1, 4, 8, 9 };
        ra::Unique<double, 2> a({3, 2}, { 2, 3, 1, 4, 8, 9 });
        // ra::Unique<double, 2> b(a); // error; need temp
        ra::Unique<double, 2> c(ra::Unique<double, 2>({3, 2}, { 2, 3, 1, 4, 8, 9 })); // ok; from actual temp
        tr.test(std::equal(check, check+6, c.begin()));
        ra::Unique<double, 2> d(std::move(a)); // ok; from fake temp
        tr.test(std::equal(check, check+6, d.begin()));
    }
    tr.section("construct from xpr having its own shape");
    {
        ra::Unique<double, 0> a(ra::scalar(33));
        ra::Unique<double> b(ra::scalar(44));
// b.rank() is runtime, so b()==44. and the whole assert argument become array xprs when operators.hh is included.
        tr.test_eq(0, b.rank());
        tr.test_eq(1, b.size());
        tr.test_eq(44, b());
        b = 55.;
        cout << "b: " << b << endl;
// see above for b.rank().
        tr.test_eq(0, b.rank());
        tr.test_eq(1, b.size());
        tr.test_eq(55., b());
    }
    tr.section("rank 0 -> scalar with storage type");
    {
        auto rank0test0 = [](double & a) { a *= 2; };
        auto rank0test1 = [](double const & a) { return a*2; };
        ra::Unique<double, 0> a(ra::scalar(33));
        tr.test_eq(1, a.size());

// See note in (170).
        // static_assert(sizeof(a())==sizeof(double *), "bad assumption");

        rank0test0(a);
        tr.test_eq(66, a);
        double b = rank0test1(a);
        tr.test_eq(66, a);
        tr.test_eq(132, b);
    }
    tr.section("rank 0 -> scalar with storage type, explicit size");
    {
        auto rank0test0 = [](double & a) { a *= 2; };
        auto rank0test1 = [](double const & a) { return a*2; };
        ra::Unique<double, 0> a({}, ra::scalar(33));
        tr.test_eq(1, a.size());

// See note in (170).
        // static_assert(sizeof(a())==sizeof(double *), "bad assumption");
        rank0test0(a);
        tr.test_eq(66, a);
        double b = rank0test1(a);
        tr.test_eq(66, a);
        tr.test_eq(132, b);
    }
    tr.section("constructors from data in initializer_list");
    {
        double checka[6] = { 2, 3, 1, 4, 8, 9 };
        {
            ra::Unique<double, 2> a({2, 3}, { 2, 3, 1, 4, 8, 9 });
            tr.test_eq(2, a.dim[0].size);
            tr.test_eq(3, a.dim[1].size);
            tr.test(std::equal(a.begin(), a.end(), checka));
        }
        {
            ra::Unique<double> a { 2, 3, 1, 4, 8, 9 };
            tr.test_eq(6, a.size());
            tr.test_eq(1, a.rank());
            tr.test(std::equal(a.begin(), a.end(), checka));
            ra::Unique<double> b ({ 2, 3, 1, 4, 8, 9 });
            tr.test_eq(6, b.size());
            tr.test_eq(1, b.rank());
            tr.test(std::equal(b.begin(), b.end(), checka));
        }
        {
            ra::Unique<double, 1> a { 2, 3, 1, 4, 8, 9 };
            tr.test_eq(6, a.size());
            tr.test_eq(1, a.rank());
            tr.test(std::equal(a.begin(), a.end(), checka));
            ra::Unique<double, 1> b ({ 2, 3, 1, 4, 8, 9 });
            tr.test_eq(6, b.size());
            tr.test_eq(1, b.rank());
            tr.test(std::equal(b.begin(), b.end(), checka));
        }
    }
    tr.section("row-major assignment from initializer_list, rank 2");
    {
        ra::Unique<double, 2> a({3, 2}, ra::none);
        a = { 2, 3, 1, 4, 8, 9 };
        tr.test_eq(2, a(0, 0));
        tr.test_eq(3, a(0, 1));
        tr.test_eq(1, a(1, 0));
        tr.test_eq(4, a(1, 1));
        tr.test_eq(8, a(2, 0));
        tr.test_eq(9, a(2, 1));

        auto b = transpose({1, 0}, a);
        b = { 2, 3, 1, 4, 8, 9 };
        tr.test_eq(2, b(0, 0));
        tr.test_eq(3, b(0, 1));
        tr.test_eq(1, b(0, 2));
        tr.test_eq(4, b(1, 0));
        tr.test_eq(8, b(1, 1));
        tr.test_eq(9, b(1, 2));

        tr.test_eq(2, a(0, 0));
        tr.test_eq(4, a(0, 1));
        tr.test_eq(3, a(1, 0));
        tr.test_eq(8, a(1, 1));
        tr.test_eq(1, a(2, 0));
        tr.test_eq(9, a(2, 1));

        auto c = transpose({1, 0}, a);
        tr.test(a.data()==c.data()); // pointers are not ra::scalars. Dunno if this deserves fixing.
        tr.test_eq(a.size(0), c.size(1));
        tr.test_eq(a.size(1), c.size(0));
        tr.test_eq(b, c);
    }
    tr.section("row-major assignment from initializer_list, rank 1");
    {
        ra::Big<double, 1> a({5}, ra::none);
        a = { 2, 3, 1, 4, 8 };
        tr.test_eq(2, a(0));
        tr.test_eq(3, a(1));
        tr.test_eq(1, a(2));
        tr.test_eq(4, a(3));
        tr.test_eq(8, a(4));
    }
    tr.section("subscripts");
    {
        tr.section("View fixed rank == 0");
        {
            double x = 99;
            ra::View<double, 0> y(ra::Small<int, 0>{}, &x);
            tr.test_eq(99, y());
            tr.test_eq(99, y);
            double u = 77.;
            ra::View<double, 0> v(ra::Small<int, 0>{}, &u);
            y = v;
            tr.test_eq(77, u);
            tr.test_eq(77, v);
            tr.test_eq(77, x);
            tr.test_eq(77, y);
        }
        tr.section("View fixed rank > 0");
        {
            double rpool[6] = { 1, 2, 3, 4, 5, 6 };
            ra::View<double, 2> r { {ra::Dim {3, 1}, ra::Dim {2, 3}}, rpool };
            cout << "org" << endl;
            std::copy(r.begin(), r.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
            {
                double rcheck[6] = { 1, 4, 2, 5, 3, 6 };
                auto r0 = r();
                tr.test(std::equal(r0.begin(), r0.end(), rcheck));
                ra::Small<int, 0> i0 {};
                tr.info("ra::Small<int, 0> rank").test_eq(1, i0.rank());
                auto r0a = r.at(ra::Small<int, 0> {});
                tr.info("fix size").test(std::equal(r0a.begin(), r0a.end(), rcheck));
                auto r0b = r.at(ra::Big<int, 1> {});
                tr.info("fix rank").test(std::equal(r0b.begin(), r0b.end(), rcheck));
                auto r0c = r.at(0+ra::Big<int, 1> {});
                tr.info("fix rank expr").test(std::equal(r0c.begin(), r0c.end(), rcheck));
                auto r0d = r.at(0+ra::Big<int>({0}, {}));
                tr.info("r0d: [", r0d, "]").test(std::equal(r0d.begin(), r0d.end(), rcheck));
            }
            {
                double rcheck[2] = { 2, 5 };
                auto r1 = r(1);
                tr.test_eq(ra::ptr(rcheck), r1);
                auto r1a = r.at(ra::Small<int, 1> {1});
                tr.test_eq(ra::ptr(rcheck), r1a);
                auto r1b = r.at(ra::Big<int, 1> {1});
                tr.test_eq(ra::ptr(rcheck), r1b);
                auto r1c = r.at(0+ra::Big<int, 1> {1});
                tr.test_eq(ra::ptr(rcheck), r1c);
                auto r1d = r.at(0+ra::Big<int> {1});
                tr.test_eq(ra::ptr(rcheck), r1d);
            }
            {
                double rcheck[2] = { 5 };

                // Does r(1, 1) return rank 0, or a scalar directly?
                // std::copy(r2.begin(), r2.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
                // tr.test(std::equal(r2.begin(), r2.end(), rcheck));
                auto r2 = r(1, 1);
                tr.test_eq(5, r2);

                auto r2a = r.at(ra::Small<int, 2> {1, 1});
                tr.info("r2a 1)").test(std::equal(r2a.begin(), r2a.end(), rcheck));
                auto r2b = r.at(ra::Big<int, 1> {1, 1});
                tr.info("r2a 2)").test(std::equal(r2a.begin(), r2a.end(), rcheck));
                auto r2c = r.at(0+ra::Big<int, 1> {1, 1});
                tr.info("r2a 3)").test(std::equal(r2c.begin(), r2c.end(), rcheck));
                auto r2d = r.at(0+ra::Big<int> {1, 1});
                tr.info("r2a 4)").test(std::equal(r2d.begin(), r2d.end(), rcheck));
            }
        }
        // TODO Subscript a rank>1 array, multiple selectors, mixed beatable & unbeatable selectors.
        tr.section("View fixed rank, unbeatable subscripts");
        {
            ra::Unique<double, 1> a = {1, 2, 3, 4};
            ra::Unique<int, 1> i = {3, 1, 2};
            cout << a(i) << endl;
            ra::Unique<double, 1> ai = a(i);
            tr.test_eq(i.size(), ai.size());
            tr.test_eq(a[i[0]], ai[0]);
            tr.test_eq(a[i[1]], ai[1]);
            tr.test_eq(a[i[2]], ai[2]);
            a(i) = ra::Unique<double, 1> {7, 8, 9};
            cout << a << endl;
            tr.test_eq(4, a.size());
            tr.test_eq(1, a[0]);
            tr.test_eq(a[i[0]], 7);
            tr.test_eq(a[i[1]], 8);
            tr.test_eq(a[i[2]], 9);
        }
        tr.section("View var rank");
        {
            double rpool[6] = { 1, 2, 3, 4, 5, 6 };
            ra::View<double> r { {ra::Dim {3, 1}, ra::Dim {2, 3}}, rpool };
            tr.test_eq(2, r.rank());
            cout << "org" << endl;
            std::copy(r.begin(), r.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;

            double rcheck0[6] = { 1, 4, 2, 5, 3, 6 };
            auto r0 = r();
            auto r0a = r.at(ra::Small<int, 0> {});
            tr.test_eq(2, r0a.rank());
            tr.test_eq(2, r0.rank());
            cout << "r0" << endl;
            std::copy(r0.begin(), r0.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
            tr.test(std::equal(r0.begin(), r0.end(), rcheck0));
            tr.test(std::equal(r0a.begin(), r0a.end(), rcheck0));

            double rcheck1[2] = { 2, 5 };
            auto r1 = r(1);
            auto r1a = r.at(ra::Small<int, 1> {1});
            tr.test_eq(1, r1a.rank());
            tr.test_eq(1, r1.rank());
            cout << "r1" << endl;
            std::copy(r1.begin(), r1.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
            tr.test(std::equal(r1.begin(), r1.end(), rcheck1));
            tr.test(std::equal(r1a.begin(), r1a.end(), rcheck1));

            double rcheck2[2] = { 5 };
            auto r2 = r(1, 1);
            auto r2a = r.at(ra::Small<int, 2> {1, 1});
            tr.test_eq(0, r2a.rank());
            cout << "r2" << endl;
            std::copy(r2.begin(), r2.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
            tr.test(std::equal(r2.begin(), r2.end(), rcheck2));
            tr.test(std::equal(r2a.begin(), r2a.end(), rcheck2));
        }
// TODO Make sure that this is double & = 99, etc. and not View<double, 0> = 99, etc.
        tr.section("assign to rank-0 result of subscript");
        {
            double check[6] = {99, 88, 77, 66, 55, 44};
            ra::Unique<double> a({2, 3}, 11.);
            a(0, 0) = 99; a(0, 1) = 88; a(0, 2) = 77;
            a(1, 0) = 66; a(1, 1) = 55; a(1, 2) = 44;
            std::copy(a.begin(), a.end(), std::ostream_iterator<double>(cout, " ")); cout << endl;
            tr.test(std::equal(check, check+6, a.begin()));
        }
    }
    tr.section("construct from shape");
    {
        ra::Unique<double> a(std::vector<ra::dim_t> {3, 2, 4}, ra::none);
        std::iota(a.begin(), a.end(), 0);
        auto sa = ra::shape(a);
        tr.test_eq(3, sa[0]);
        tr.test_eq(2, sa[1]);
        tr.test_eq(4, sa[2]);
        double check[24];
        std::iota(check, check+24, 0);
        tr.test(std::equal(check, check+24, a.begin()));
    }
    tr.section("I/O");
    {
        tr.section("1");
        {
            ra::Small<double, 3, 2> s { 1, 4, 2, 5, 3, 6 };
            double check[6] = { 1, 4, 2, 5, 3, 6 };
            CheckArrayIO(tr, s, check);
        }
        tr.section("2");
        {
            ra::Small<double, 3> s { 1, 4, 2 };
            double check[3] = { 1, 4, 2 };
            CheckArrayIO(tr, s, check);
        }
        tr.section("3");
        {
            ra::Small<double> s { 77 };
            double check[1] = { 77 };
            CheckArrayIO(tr, s, check);
        }
        tr.section("4. View<> can't allocate, so have no istream >>. Check output only.");
        {
            double rpool[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
            ra::View<double, 3> r { {ra::Dim {2, 4}, ra::Dim {2, 2}, ra::Dim {2, 1}}, rpool };
            double check[11] = { 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8 };
            CheckArrayOutput(tr, r, check);
        }
        tr.section("5");
        {
            double rpool[6] = { 1, 2, 3, 4, 5, 6 };
            ra::View<double, 2> r { {ra::Dim {3, 1}, ra::Dim {2, 3}}, rpool };
            double check[8] = { 3, 2, 1, 4, 2, 5, 3, 6 };
            CheckArrayOutput(tr, r, check);
        }
        tr.section("6");
        {
            double rpool[3] = { 1, 2, 3 };
            ra::View<double, 1> r { {ra::Dim {3, 1}}, rpool };
            double check[4] = { 3, 1, 2, 3 };
            CheckArrayOutput(tr, r, check);
        }
        tr.section("7");
        {
            double rpool[1] = { 88 };
            ra::View<double, 0> r { {}, rpool };
            double check[1] = { 88 };
            CheckArrayOutput(tr, r, check);
            tr.test_eq(1, r.size());
// See note in (170).
            // static_assert(sizeof(r)==sizeof(double *), "bad assumption");
            tr.test_eq(88, r);
        }
        tr.section("8");
        {
            double rpool[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
            ra::View<double> a { {ra::Dim {2, 4}, ra::Dim {2, 2}, ra::Dim {2, 1}}, rpool };
            double check[12] = { 3, 2, 2, 2, 1, 2, 3, 4, 5, 6, 7, 8 };
            CheckArrayOutput(tr, a, check);
// default strides.
            ra::View<double> b { {2, 2, 2}, rpool };
            CheckArrayOutput(tr, b, check);
        }
        tr.section("9");
        {
            ra::Unique<double, 3> a(std::vector<ra::dim_t> {3, 2, 4}, ra::none);
            std::iota(a.begin(), a.end(), 0);
            double check[3+24] = { 3, 2, 4 };
            std::iota(check+3, check+3+24, 0);
            CheckArrayIO(tr, a, check);
        }
        tr.section("10");
        {
            ra::Unique<double> a(std::vector<ra::dim_t> {3, 2, 4}, ra::none);
            std::iota(a.begin(), a.end(), 0);
            double check[4+24] = { 3, 3, 2, 4 };
            std::iota(check+4, check+4+24, 0);
            CheckArrayIO(tr, a, check);
        }
    }
    tr.section("ply - xpr types - Scalar");
    {
        {
            auto s = ra::scalar(7);
            cout << "s: " << s.c << endl;
        }
        {
            auto s = ra::scalar(ra::Small<int, 2> {11, 12});
            cout << "s: " << s.c << endl;
        }
        {
            ra::Unique<double> a(std::vector<ra::dim_t> {3, 2, 4}, ra::none);
            std::iota(a.begin(), a.end(), 0);
            auto s = ra::scalar(a);
            cout << "s: " << s.c << endl;
        }
    }
    tr.section("scalar as reference");
    {
        int a = 3;
        ra::scalar(a) += ra::Small<int, 3> {4, 5, 6};
        tr.test_eq(18, a);
        // beware: throws away 3+4,3+5, only 3+6 is left. [ma107]
        ra::scalar(a) = 3 + ra::Small<int, 3> {4, 5, 6};
        tr.test_eq(9, a);
    }
    tr.section("ra::iota");
    {
        static_assert(ra::RaIterator<decltype(ra::iota(10))>, "bad type pred for iota");
        tr.section("straight cases");
        {
            ra::Big<int, 1> a = ra::iota(4, 1);
            assert(a[0]==1 && a[1]==2 && a[2]==3 && a[3]==4);
        }
        tr.section("work with operators");
        {
            tr.test(every(ra::iota(4)==ra::Big<int, 1> {0, 1, 2, 3}));
            tr.test(every(ra::iota(4, 1)==ra::Big<int, 1> {1, 2, 3, 4}));
            tr.test(every(ra::iota(4, 1, 2)==ra::Big<int, 1> {1, 3, 5, 7}));
        }
 // TODO actually whether unroll is avoided depends on ply(), have a way to require it [ra03]
        tr.section("frame-matching, forbidding unroll");
        {
            ra::Big<int, 3> b ({3, 4, 2}, ra::none);
            transpose({0, 2, 1}, b) = ra::iota(3, 1);
            cout << b << endl;
            tr.test(every(b(0)==1));
            tr.test(every(b(1)==2));
            tr.test(every(b(2)==3));
        }
        {
            ra::Big<int, 3> b ({3, 4, 2}, ra::none);
            transpose<0, 2, 1>(b) = ra::iota(3, 1);
            cout << b << endl;
            tr.test(every(b(0)==1));
            tr.test(every(b(1)==2));
            tr.test(every(b(2)==3));
        }
    }
    return tr.summary();
}
