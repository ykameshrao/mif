//-------------------------------------------------------------------
//  MetaInfo Framework (MIF)
//  https://github.com/tdv/mif
//  Created:     10.2016
//  Copyright (C) 2016-2018 tdv
//-------------------------------------------------------------------

#ifndef __MIF_COMMON_STATIC_STRING_H__
#define __MIF_COMMON_STATIC_STRING_H__

// STD
#include <cstdint>

// MIF
#include "mif/common/index_sequence.h"

#define MIF_STR_IMPL_1(str_, i_) \
    (sizeof(str_) > (i_) ? str_[(i_)] : 0)

#define MIF_STR_IMPL_4(str_, i_) \
    MIF_STR_IMPL_1(str_, i_ + 0), \
    MIF_STR_IMPL_1(str_, i_ + 1), \
    MIF_STR_IMPL_1(str_, i_ + 2), \
    MIF_STR_IMPL_1(str_, i_ + 3)

#define MIF_STR_IMPL_16(str_, i_) \
    MIF_STR_IMPL_4(str_, i_ + 0), \
    MIF_STR_IMPL_4(str_, i_ + 4), \
    MIF_STR_IMPL_4(str_, i_ + 8), \
    MIF_STR_IMPL_4(str_, i_ + 12)

#define MIF_STR_IMPL_64(str_, i_) \
    MIF_STR_IMPL_16(str_, i_ + 0), \
    MIF_STR_IMPL_16(str_, i_ + 16), \
    MIF_STR_IMPL_16(str_, i_ + 32), \
    MIF_STR_IMPL_16(str_, i_ + 48)

#define MIF_STR(str_) \
    MIF_STR_IMPL_64(str_, 0), 0

#define MIF_STATIC_STR(str_) \
    ::Mif::Common::MakeStaticString<MIF_STR(str_)>

namespace Mif
{
    namespace Common
    {

        template <char ... Str>
        struct StaticString
        {
            static constexpr char Value[sizeof ... (Str)] = { Str ... };
            static constexpr std::size_t Size = sizeof ... (Str);
        };

        template <char ... Str>
        extern constexpr char StaticString<Str ... >::Value[sizeof ... (Str)];

        namespace Detail
        {

            template <char Ch, char ... Str>
            struct CharArraySize
            {
                static constexpr std::size_t Size = CharArraySize<Str ... >::Size + 1;
            };

            template <char ... Str>
            struct CharArraySize<0, Str ... >
            {
                static constexpr std::size_t Size = 1;
            };

            template <char ... Str, std::size_t ... Indexes>
            constexpr StaticString<StaticString<Str ... >::Value[Indexes] ... >
            MakeStaticString(IndexSequence<Indexes ... > const *);

        }   // namespace Detail

        template <char ... Str>
        using MakeStaticString = decltype(Detail::MakeStaticString<Str ... >(
            static_cast<MakeIndexSequence<Detail::CharArraySize<Str ... , 0>::Size> const *>(nullptr)));

    }   // namespace Common
}   // namespace Mif


#endif  // !__MIF_COMMON_STATIC_STRING_H__
