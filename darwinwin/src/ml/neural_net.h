#pragma once

#include "core.h"

constexpr size_t neural_net_block_size = sizeof(__m256) / sizeof(int16_t);

template <size_t layer_blocks_, size_t layers_>
struct neural_net
{
  static constexpr size_t layer_blocks = layer_blocks_;
  static constexpr size_t layers = layers_;

  static constexpr size_t block_size = neural_net_block_size;
  static constexpr size_t neurons_per_layer = layer_blocks * block_size;
  static constexpr size_t weights_per_neuron = neurons_per_layer;
  static constexpr size_t weights_per_layer = neurons_per_layer * neurons_per_layer;
  static constexpr size_t biases_per_layer = neurons_per_layer;

  // data layout:
  //   sequential layers with:
  //     weights_per_neuron weights for each neuron
  //     biases_per_layer bias values (one for each neuron)
  LS_ALIGN(32) int16_t data[(weights_per_layer + biases_per_layer) * layers];
};

template <size_t layer_blocks>
struct neural_net_buffer
{
  static constexpr size_t block_size = neural_net_block_size;
  LS_ALIGN(32) int16_t data[layer_blocks * block_size];
};

// convert any non-zero values to `lsMaxValue<int8_t>()` => ~1 in fixed point.
template <size_t layer_blocks>
inline void neural_net_buffer_prepare(neural_net_buffer<layer_blocks> &b, const size_t blockCount = layer_blocks)
{
  lsAssert(blockCount <= layer_blocks);

  __m256i *pBuffer = reinterpret_cast<__m256i *>(b.data);
  const __m256i expected = _mm256_set1_epi16(lsMaxValue<int8_t>());

  for (size_t inputBlock = 0; inputBlock < blockCount; inputBlock++)
  {
    const __m256i raw = _mm256_load_si256(pBuffer + inputBlock);
    const __m256i cmp = _mm256_cmpeq_epi16(_mm256_cmpeq_epi16(raw, _mm256_setzero_si256()), _mm256_setzero_si256());
    const __m256i out = _mm256_and_si256(cmp, expected);
    _mm256_store_si256(pBuffer + inputBlock, out);
  }
}

template <size_t layer_blocks, size_t layers>
inline void neural_net_eval(const neural_net<layer_blocks, layers> &nn, neural_net_buffer<layer_blocks> &io)
{
  LS_ALIGN(32) int16_t tmp[layer_blocks * nn.block_size] = {};

  __m256i *pIO = reinterpret_cast<__m256i *>(io.data);
  __m256i *pTmp = reinterpret_cast<__m256i *>(tmp);
  const __m256i *pLayer = reinterpret_cast<const __m256i *>(nn.data);
  const __m128i _FFFF_64 = _mm_set1_epi64x(0xFFFF);
  const __m256i _min_16 = _mm256_set1_epi8(lsMinValue<int8_t>());
  const __m256i _max_16 = _mm256_set1_epi8(lsMaxValue<int8_t>());

  for (size_t layer = 0; layer < layers; layer++)
  {
    // Accumulate Weights.
    for (size_t neuron = 0; neuron < nn.neurons_per_layer; neuron++)
    {
      for (size_t inputBlock = 0; inputBlock < nn.layer_blocks; inputBlock++)
      {
        const __m256i weight = _mm256_load_si256(pLayer);
        pLayer++;

        const __m256i in = _mm256_load_si256(pIO);

        const __m256i resRaw = _mm256_mullo_epi16(weight, in);
        const __m256i resNormalized = _mm256_srai_epi16(resRaw, 7);

        const __m256i resAdd2 = _mm256_hadds_epi16(resNormalized, resNormalized); // ACEG....IKMO....
        const __m256i resAdd4 = _mm256_hadds_epi16(resAdd2, resAdd2); // AE......IM......
        const __m256i resAdd8 = _mm256_hadds_epi16(resAdd4, resAdd4); // A.......I.......

        tmp[neuron] += (int16_t)_mm256_extract_epi16(resAdd8, 0) + (int16_t)_mm256_extract_epi16(resAdd8, 8);
      }
    }

    for (size_t inputBlock = 0; inputBlock < layer_blocks; inputBlock++)
    {
      const __m256i bias = _mm256_load_si256(pLayer);
      pLayer++;

      const __m256i weightSum = _mm256_load_si256(pTmp + inputBlock);

      const __m256i sum = _mm256_adds_epi16(bias, weightSum);
      const __m256i res = _mm256_max_epi16(_mm256_min_epi16(sum, _max_16), _min_16);

      _mm256_store_si256(pIO + inputBlock, res);
    }
  }
}