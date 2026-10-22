#include <math.h>
#include <stdio.h>
#include "depthwise_channel_fixture.h"

/*
 * One channel of real MobileNetV2 Conv_75:
 * input 14x14, weights 3x3, output 7x7,
 * stride 2, padding 1.
 *
 * All loop bounds and array sizes are fixed.
 */
__attribute__((noinline))
void depthwise_channel(
    const float input[196],
    const float weights[9],
    float output[49])
{
    for (int oy = 0; oy < 7; ++oy) {
        for (int ox = 0; ox < 7; ++ox) {
            float accumulator = 0.0f;

            for (int ky = 0; ky < 3; ++ky) {
                for (int kx = 0; kx < 3; ++kx) {
                    int iy = oy * 2 + ky - 1;
                    int ix = ox * 2 + kx - 1;

                    if (iy >= 0 && iy < 14 &&
                        ix >= 0 && ix < 14) {
                        accumulator +=
                            input[iy * 14 + ix] *
                            weights[ky * 3 + kx];
                    }
                }
            }

            output[oy * 7 + ox] = accumulator;
        }
    }
}

int main(void)
{
    float actual[49] = {0};
    float maximum_error = 0.0f;
    int incorrect = 0;

    depthwise_channel(
        test_input, test_weights, actual
    );

    for (int i = 0; i < 49; ++i) {
        float error = fabsf(
            actual[i] - expected_output[i]
        );

        if (!isfinite(actual[i])) {
            ++incorrect;
        }

        if (error > maximum_error) {
            maximum_error = error;
        }

        if (error > 1e-5f) {
            ++incorrect;
        }
    }

    printf("Layer: Conv_75/WithoutBiases\n");
    printf("Real model weights: YES\n");
    printf("Output values checked: 49\n");
    printf("Maximum absolute error: %.9g\n",
           (double)maximum_error);

    if (incorrect == 0) {
        puts("COMPILED DEPTHWISE VALIDATION: PASS");
        return 0;
    }

    puts("COMPILED DEPTHWISE VALIDATION: FAIL");
    return 1;
}
