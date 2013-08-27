// Copyright (C) by Josh Blum. See LICENSE.txt for licensing information.

#include <gras/block.hpp>
#include <gras/factory.hpp>
#include <stdexcept>
#include <complex>
#include <vector>
#include <boost/cstdint.hpp>
#ifdef HAVE_VOLK
#include <volk/volk.h>
#endif

/***********************************************************************
 * Generic multiply const implementation
 **********************************************************************/
template <typename type>
struct MultiplyConst : gras::Block
{
    MultiplyConst(const std::vector<type> &vec, const bool vec_mode = true):
        gras::Block("GrEx MultiplyConst")
    {
        this->input_config(0).item_size = sizeof(type)*vec.size();
        this->output_config(0).item_size = sizeof(type)*vec.size();
        _val.resize(vec.size());
        if (vec_mode)
        {
            this->register_call("set_const", &MultiplyConst<type>::set_const_v);
            this->register_call("get_const", &MultiplyConst<type>::get_const_v);
        }
        else
        {
            this->register_call("set_const", &MultiplyConst<type>::set_const);
            this->register_call("get_const", &MultiplyConst<type>::get_const);
        }
        this->set_const_v(vec);
    }

    void notify_topology(const size_t num_inputs, const size_t)
    {
        for (size_t i = 0; i < num_inputs; i++)
        {
            this->input_config(i).inline_buffer = (i == 0);
        }
    }

    void set_const_v(const std::vector<type> &v)
    {
        _val = v;
    }

    std::vector<type> get_const_v(void)
    {
        return _val;
    }

    void set_const(const type &val)
    {
        this->set_const_v(std::vector<type >(1, val));
    }

    type get_const(void)
    {
        return this->get_const_v().front();
    }

    void work1(const InputItems &, const OutputItems &);

    void work(
        const InputItems &ins,
        const OutputItems &outs
    ){
        if (_val.size() == 1) return work1(ins, outs);

        const size_t n_nums = std::min(ins.min(), outs.min());
        type *out = outs[0].cast<type *>();
        const type *in = ins[0].cast<const type *>();

        for (size_t i = 0; i < n_nums * _val.size(); i++)
        {
            out[i] = in[i] * _val[i%_val.size()];
        }

        this->consume(n_nums);
        this->produce(n_nums);
    }

    std::vector<type> _val;
};

/***********************************************************************
 * Generic vlen == 1 multiplier implementation
 **********************************************************************/
template <typename type>
void MultiplyConst<type>::work1(
    const InputItems &ins, const OutputItems &outs
){
    const size_t n_nums = std::min(ins.min(), outs.min());
    type *out = outs[0].cast<type *>();
    const type *in = ins[0].cast<const type *>();

    for (size_t i = 0; i < n_nums; i++)
    {
        out[i] = in[i] * _val[0];
    }

    this->consume(n_nums);
    this->produce(n_nums);
}

#ifdef HAVE_VOLK
/***********************************************************************
 * FC32 vlen == 1 multiplier implementation
 **********************************************************************/
template <>
void MultiplyConst<std::complex<float> >::work1(
    const InputItems &ins, const OutputItems &outs
){
    const size_t n_nums = std::min(ins.min(), outs.min());
    typedef std::complex<float> type;
    const type scalar = reinterpret_cast<const type *>(&_val[0])[0];
    type *out = outs[0].cast<type *>();
    const type *in = ins[0].cast<const type *>();
    volk_32fc_s32fc_multiply_32fc(out, in, scalar, n_nums);

    this->consume(n_nums);
    this->produce(n_nums);
}

/***********************************************************************
 * F32 vlen == 1 multiplier implementation
 **********************************************************************/
template <>
void MultiplyConst<float>::work1(
    const InputItems &ins, const OutputItems &outs
){
    const size_t n_nums = std::min(ins.min(), outs.min());
    typedef float type;
    const type scalar = reinterpret_cast<const type *>(&_val[0])[0];
    type *out = outs[0].cast<type *>();
    const type *in = ins[0].cast<const type *>();
    volk_32f_s32f_multiply_32f(out, in, scalar, n_nums);

    this->consume(n_nums);
    this->produce(n_nums);
}
#endif

/***********************************************************************
 * factory function
 **********************************************************************/
#define make_factory_function(suffix, type) \
static gras::Block *make_multiply_const_v_ ## suffix(const std::vector<type > &vec) \
{ \
    return new MultiplyConst<type>(vec, true); \
} \
GRAS_REGISTER_FACTORY("/ex/multiply_const_v_" #suffix, make_multiply_const_v_ ## suffix) \
static gras::Block *make_multiply_const_ ## suffix(const type &value) \
{ \
    return new MultiplyConst<type>(std::vector<type>(1, value), false); \
} \
GRAS_REGISTER_FACTORY("/ex/multiply_const_" #suffix, make_multiply_const_ ## suffix)

make_factory_function(fc32_fc32, std::complex<float>)
make_factory_function(sc32_sc32, std::complex<boost::int32_t>)
make_factory_function(sc16_sc16, std::complex<boost::int16_t>)
make_factory_function(sc8_sc8, std::complex<boost::int8_t>)
make_factory_function(f32_f32, float)
make_factory_function(s32_s32, boost::int32_t)
make_factory_function(s16_s16, boost::int16_t)
make_factory_function(s8_s8, boost::int8_t)
