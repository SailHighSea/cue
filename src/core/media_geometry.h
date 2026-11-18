#pragma once
#include <cstdint>
#include <utility>

struct VideoShape {
  int64_t width=0,height=0,rotation=0;
  bool valid() const { return width>0 && height>0; }
  double aspect() const {
    if(!valid()) return 0;
    double w=(double)width,h=(double)height;
    if((rotation%180+180)%180==90) std::swap(w,h);
    return w/h;
  }
};

// A film's shape from what its file says, before its first picture: the demuxer's size, its
// pixels' shape (anamorphic films) and how it is turned. The engine lists "demux-rotation" only
// for a turned film (a phone video): when it says nothing, the film is not turned. (1.4 waited
// for the rotation to be said, so a plain film never got its window before its first picture.)
inline VideoShape demux_shape(int width, int height, double par, int rotation) {
  if (width <= 0 || height <= 0) return {};
  const double w = width * (par > 0 && par < 100 ? par : 1.0);
  if (!(w >= 1)) return {};
  return {(int64_t)(w + 0.5), height, rotation > 0 ? rotation : 0};
}

// A single reply owns the complete width/height/rotation tuple for this load.
// Unchanged dimensions need not generate observer events between two files.
class MediaGeometry {
 public:
  void reset() { ++request_; shape_={}; }
  uint64_t request() { return ++request_; }
  uint64_t current_request() const { return request_; }
  bool accept(uint64_t request,VideoShape shape) {
    if(request!=request_ || !shape.valid()) return false;
    shape_=shape; return true;
  }
  VideoShape shape() const { return shape_; }
 private:
  uint64_t request_=1ull<<52;
  VideoShape shape_{};
};
