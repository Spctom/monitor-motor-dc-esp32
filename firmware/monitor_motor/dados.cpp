#include "dados.h"

namespace dados {
namespace {

struct Media {
  float soma = 0;
  uint32_t n = 0;
  void somar(float v) {
    if (!isnan(v)) {
      soma += v;
      n++;
    }
  }
  float valor() const { return n ? soma / n : NAN; }
};

portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
Amostra ultima;
Media mRpm, mRef, mTemp, mCorrente, mVib, mDuty;

}  // namespace

void publicar(const Amostra& a) {
  portENTER_CRITICAL(&mux);
  ultima = a;
  mRpm.somar(a.rpm);
  mRef.somar(a.rpmRef);
  mTemp.somar(a.temperatura);
  mCorrente.somar(a.corrente);
  mVib.somar(a.vibracao);
  mDuty.somar(a.duty);
  portEXIT_CRITICAL(&mux);
}

Amostra atual() {
  portENTER_CRITICAL(&mux);
  const Amostra copia = ultima;
  portEXIT_CRITICAL(&mux);
  return copia;
}

Amostra mediaDoPeriodo() {
  portENTER_CRITICAL(&mux);
  Amostra m = ultima;
  m.rpm = mRpm.valor();
  m.rpmRef = mRef.valor();
  m.temperatura = mTemp.valor();
  m.corrente = mCorrente.valor();
  m.vibracao = mVib.valor();
  m.duty = mDuty.valor();
  mRpm = Media();
  mRef = Media();
  mTemp = Media();
  mCorrente = Media();
  mVib = Media();
  mDuty = Media();
  portEXIT_CRITICAL(&mux);
  return m;
}

}  // namespace dados
