#include <math.h>
#include <sndfile.h>
#include <string.h>
#include <stdlib.h>

#include "./ebur128.h"

#define CHECK_ERROR(condition, message, errorcode, goto_point)                 \
  if ((condition)) {                                                           \
    fprintf(stderr, message);                                                  \
    errcode = (errorcode);                                                     \
    goto goto_point;                                                           \
  }


int main(int ac, const char* av[]) {
  SF_INFO file_info;
  SNDFILE* file;
  sf_count_t nr_frames_read;
  sf_count_t nr_frames_read_all = 0;
  int errcode = 0;
  int result;

  CHECK_ERROR(ac != 2, "usage: r128-test FILENAME\n", 1, exit)

  memset(&file_info, '\0', sizeof(file_info));
  file = sf_open(av[1], SFM_READ, &file_info);
  CHECK_ERROR(!file, "Could not open input file!\n", 1, exit)

  ebur128_state st;
  ebur128_init(&st, file_info.frames, file_info.channels);

  while ((nr_frames_read = sf_readf_double(file, st.audio_data +
                                                 st.audio_data_half * 19200 *
                                                 st.channels,
                                           19200))) {
    nr_frames_read_all += nr_frames_read;
    result = ebur128_do_stuff(&st, (size_t) nr_frames_read);
    CHECK_ERROR(result, "Calculation failed!\n", 1, close_file)

    if (st.audio_data_half == 0) {
      if (st.zg_index != 0) {
        if (nr_frames_read < 9600) break;
        memcpy(st.audio_data + 19200 * st.channels,
               st.audio_data,
               9600 * st.channels * sizeof(double));
        calc_gating_block(st.audio_data + 19200 * st.channels,
                          nr_frames_read, st.channels,
                          st.zg, st.zg_index);
        ++st.zg_index;
      }
      if (nr_frames_read < 19200) break;
      calc_gating_block(st.audio_data, nr_frames_read, st.channels,
                        st.zg, st.zg_index);
      ++st.zg_index;
    } else {
      if (nr_frames_read < 9600) break;
      calc_gating_block(st.audio_data + 9600 * st.channels,
                        nr_frames_read, st.channels,
                        st.zg, st.zg_index);
      ++st.zg_index;
      if (nr_frames_read < 19200) break;
      calc_gating_block(st.audio_data + 19200 * st.channels,
                        nr_frames_read, st.channels,
                        st.zg, st.zg_index);
      ++st.zg_index;
    }

    st.audio_data_half = st.audio_data_half ? 0 : 1;
  }
  CHECK_ERROR(file_info.frames != nr_frames_read_all,
              "Could not read full file!\n", 1, close_file)

  double relative_threshold, gated_loudness;
  calculate_block_loudness(st.lg, st.zg, st.frames, st.channels);
  calculate_relative_threshold(&relative_threshold, st.lg, st.zg, st.frames, st.channels);
  calculate_gated_loudness(&gated_loudness, relative_threshold, st.lg, st.zg, st.frames, st.channels);

  fprintf(stderr, "relative threshold: %f LKFS\n", relative_threshold);
  fprintf(stderr, "gated loudness: %f LKFS\n", gated_loudness);


/*
free_lg:
  free(lg);

free_zg:
  release_filter_state(&zg, file_info.channels);

free_z:
  free(z);

release_filter_state_2:
  release_filter_state(&v2, file_info.channels);

release_filter_state_1:
  release_filter_state(&v, file_info.channels);

free_audio_data:
  free(audio_data);
*/

close_file:
  if (sf_close(file)) {
    fprintf(stderr, "Could not close input file!\n");
  }

exit:
  return errcode;
}
