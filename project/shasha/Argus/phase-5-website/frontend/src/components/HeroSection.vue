<template>
  <section class="hero" id="hero">
    <div class="shark-wrap">
      <img src="/assets/images/shabao_laugh.gif" alt="鲨宝" width="220">
    </div>
    <h1 class="hero-logo">
      <span class="cn">鲨宝 Official</span>
      <span class="en">Shabao · VTuber</span>
    </h1>
    <p class="slogan">游进你的心里，<span class="hl">陪你每一场直播</span> 🌊</p>
    <a href="https://live.bilibili.com/31218790" class="btn-live" target="_blank" rel="noopener">
      <span class="pulse"></span>
      立即前往直播间
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
        <path d="M5 12h14M13 6l6 6-6 6" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round"/>
      </svg>
    </a>
    <div class="scroll-hint">
      <span>向下探索</span>
      <svg width="16" height="16" viewBox="0 0 24 24" fill="none">
        <path d="M6 9l6 6 6-6" stroke="rgba(255,255,255,.4)" stroke-width="2" stroke-linecap="round"/>
      </svg>
    </div>
  </section>
</template>

<script setup>
</script>

<style scoped>
.hero {
  position: relative;
  z-index: 1;
  min-height: 100vh;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  text-align: center;
  padding: 6rem 2rem 4rem;
}

.shark-wrap {
  position: relative;
  animation: float 4s ease-in-out infinite;
  margin-bottom: 1.8rem;
  filter: drop-shadow(0 20px 60px rgba(0,200,255,.3));
}

@keyframes float {
  0%,100% { transform: translateY(0) rotate(-1deg) }
  50%      { transform: translateY(-18px) rotate(1deg) }
}

.hero-logo {
  font-size: clamp(2.4rem,7vw,4.8rem);
  font-weight: 900;
  line-height: 1.1;
  letter-spacing: -.01em;
  margin-bottom: .8rem;
}

.hero-logo .cn {
  background: linear-gradient(135deg,#fff 20%,var(--teal) 60%,var(--teal2));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.hero-logo .en {
  display: block;
  font-size: clamp(1rem,2.5vw,1.6rem);
  font-weight: 500;
  letter-spacing: .25em;
  color: rgba(0,200,255,.75);
  text-transform: uppercase;
  margin-top: .3rem;
}

.slogan {
  font-size: clamp(1rem,2.2vw,1.3rem);
  color: rgba(255,255,255,.65);
  letter-spacing: .08em;
  margin-bottom: 2.5rem;
  font-style: italic;
}

.slogan .hl { color: var(--teal2) }

.btn-live {
  display: inline-flex;
  align-items: center;
  gap: .7rem;
  padding: .9rem 2.4rem;
  border-radius: 999px;
  background: linear-gradient(135deg,#0090d4,var(--teal));
  color: #fff;
  font-size: 1rem;
  font-weight: 700;
  text-decoration: none;
  letter-spacing: .04em;
  box-shadow: 0 0 40px rgba(0,200,255,.45), 0 4px 20px rgba(0,0,0,.4);
  transition: transform .25s, box-shadow .25s;
  position: relative;
  overflow: hidden;
}

.btn-live::before {
  content: '';
  position: absolute;
  inset: 0;
  background: linear-gradient(135deg,rgba(255,255,255,.18),transparent);
  border-radius: inherit;
}

.btn-live:hover {
  transform: translateY(-3px) scale(1.03);
  box-shadow: 0 0 60px rgba(0,200,255,.65), 0 8px 30px rgba(0,0,0,.4);
}

.pulse {
  width: 10px;
  height: 10px;
  border-radius: 50%;
  background: #ff4a6e;
  box-shadow: 0 0 8px #ff4a6e;
  animation: pulseDot 1.4s ease-in-out infinite;
}

@keyframes pulseDot {
  0%,100% { transform: scale(1); opacity: 1 }
  50%      { transform: scale(1.5); opacity: .7 }
}

.scroll-hint {
  position: absolute;
  bottom: 2.5rem;
  left: 50%;
  transform: translateX(-50%);
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: .4rem;
  color: rgba(255,255,255,.3);
  font-size: .75rem;
  letter-spacing: .1em;
  animation: fadeUp .8s .8s both;
}

.scroll-hint svg { animation: bounce 2s ease-in-out infinite }

@keyframes bounce {
  0%,100% { transform: translateY(0) }
  50%      { transform: translateY(6px) }
}

@keyframes fadeUp {
  from { opacity: 0; transform: translateX(-50%) translateY(16px) }
  to   { opacity: 1; transform: translateX(-50%) translateY(0) }
}
</style>