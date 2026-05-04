<template>
  <div class="ocean">
    <div class="rays"></div>
    <div class="wave-overlay"></div>
    <div class="bubbles" ref="bubblesRef"></div>
    <div class="particles" ref="particlesRef"></div>
    <div class="seaweed" style="left:3%;bottom:0">
      <svg width="40" height="120" viewBox="0 0 40 120" fill="none">
        <path d="M20 120 C10 90 30 70 18 50 C8 30 25 10 20 0" stroke="rgba(0,180,100,.35)" stroke-width="3" stroke-linecap="round" fill="none">
          <animateTransform attributeName="transform" type="rotate" values="-4 20 120;4 20 120;-4 20 120" dur="4s" repeatCount="indefinite"/>
        </path>
        <path d="M22 120 C32 95 14 75 26 58 C36 40 18 20 24 5" stroke="rgba(0,180,100,.25)" stroke-width="2" stroke-linecap="round" fill="none">
          <animateTransform attributeName="transform" type="rotate" values="3 22 120;-3 22 120;3 22 120" dur="5s" repeatCount="indefinite"/>
        </path>
      </svg>
    </div>
    <div class="seaweed" style="right:5%;bottom:0">
      <svg width="50" height="140" viewBox="0 0 50 140" fill="none">
        <path d="M25 140 C12 110 38 85 22 60 C8 38 30 15 25 0" stroke="rgba(0,160,90,.3)" stroke-width="3.5" stroke-linecap="round" fill="none">
          <animateTransform attributeName="transform" type="rotate" values="4 25 140;-4 25 140;4 25 140" dur="6s" repeatCount="indefinite"/>
        </path>
      </svg>
    </div>
    <div class="sand"></div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'

const bubblesRef = ref(null)
const particlesRef = ref(null)

onMounted(() => {
  // 气泡
  for (let i = 0; i < 28; i++) {
    const b = document.createElement('div')
    b.className = 'b'
    const sz = Math.random() * 12 + 4
    const dur = Math.random() * 18 + 12
    const delay = Math.random() * 20
    const dx = (Math.random() - 0.5) * 120 + 'px'
    b.style.cssText = `width:${sz}px;height:${sz}px;left:${Math.random()*100}%;animation-duration:${dur}s;animation-delay:-${delay}s;--dx:${dx}`
    bubblesRef.value.appendChild(b)
  }
  // 粒子
  for (let i = 0; i < 60; i++) {
    const p = document.createElement('div')
    p.className = 'p'
    const dur = Math.random() * 4 + 2
    const delay = Math.random() * 6
    p.style.cssText = `left:${Math.random()*100}%;top:${Math.random()*80}%;animation-duration:${dur}s;animation-delay:-${delay}s;opacity:${Math.random()*.5+.1}`
    particlesRef.value.appendChild(p)
  }
})
</script>