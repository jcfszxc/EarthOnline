<template>
  <section class="section" id="about">
    <p class="sec-tag">About</p>
    <h2 class="sec-title">关于鲨宝</h2>
    <div class="about-grid">
      <div class="about-img-box">
        <div class="about-glow"></div>
        <img src="/assets/images/shabao_driving.gif" alt="鲨宝" width="220">
      </div>
      <div class="about-text">
        <h3>嗨！我是鲨宝 🦈</h3>
        <p>来自深海的可爱虚拟主播，平时喜欢游来游去找朋友玩耍。粉丝群:930445275 幸运不会降临在不努力的人身上 幸运值100+可抓捕</p>
        <div class="tag-list">
          <span class="tag">💬 闲聊互动</span>
          <span class="tag">🎵 歌回</span>
        </div>
      </div>
    </div>
  </section>
</template>

<style scoped>
.about-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 3rem;
  align-items: center;
}

.about-img-box {
  position: relative;
  display: flex;
  align-items: center;
  justify-content: center;
}

.about-glow {
  position: absolute;
  inset: -20px;
  border-radius: 50%;
  background: radial-gradient(ellipse, rgba(0,200,255,.2), transparent 70%);
}

.about-text h3 {
  font-size: 1.4rem;
  margin-bottom: 1rem;
}

.about-text p {
  color: var(--dim);
  line-height: 1.9;
  font-size: .95rem;
  margin-bottom: 1rem;
}

.tag-list {
  display: flex;
  flex-wrap: wrap;
  gap: .5rem;
  margin-top: 1.2rem;
}

.tag {
  padding: .35rem .9rem;
  border-radius: 999px;
  font-size: .8rem;
  background: rgba(0,200,255,.1);
  border: 1px solid rgba(0,200,255,.2);
  color: var(--teal);
}

@media (max-width: 640px) {
  .about-grid { grid-template-columns: 1fr }
}
</style>