#ifndef HOMOMORPHIC_STITCHING_OPAQUE_H
#define HOMOMORPHIC_STITCHING_OPAQUE_H

#include "Nal.h"

namespace stitching {

  class Opaque : public Nal {
  public:

      Opaque(const StitchContext &context, const bytestring &data)
              : Nal(context, data)
      { }

      /**
       *
       * @return A string wtih the bytes of this Nal
       */
      inline bytestring GetBytes() const override {
          auto data = Nal::GetBytes();
          data.insert(data.begin(), kNalMarker.begin(), kNalMarker.end());
          return data;
      }
  };

}; //namespace stitching

#endif //HOMOMORPHIC_STITCHING_OPAQUE_H
