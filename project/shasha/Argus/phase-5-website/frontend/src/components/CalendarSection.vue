<template>
  <section class="section" id="calendar">
    <p class="sec-tag">Live History</p>
    <h2 class="sec-title">直播日历</h2>

    <div class="cal-wrap">
      <!-- 月份导航 -->
      <div class="cal-header">
        <div class="cal-nav-group">
          <button class="cal-nav-btn" @click="prevMonth">‹</button>
          <button class="today-btn" @click="goToday">今天</button>
          <button class="cal-nav-btn" @click="nextMonth">›</button>
        </div>
        <div class="cal-title">{{ curYear }} 年 {{ MONTHS[curMonth] }}</div>
        <div class="cal-legend">
          <span class="legend-item">
            <span class="legend-dot" style="background:var(--teal);box-shadow:0 0 5px var(--teal)"></span>有直播
          </span>
          <span class="legend-item">
            <span class="legend-dot" style="background:rgba(0,200,255,.2);border:1px solid rgba(0,200,255,.4)"></span>今天
          </span>
          <span class="legend-note" title="单场≥30min计入，日累计≥120min计1天">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <circle cx="12" cy="12" r="10"/><path d="M12 16v-4M12 8h.01"/>
            </svg>
            有效天计算规则
          </span>
        </div>
      </div>

      <!-- 加载中 -->
      <div v-if="loading" class="cal-loading">
        <div class="spinner"></div>
        <span>正在加载直播记录…</span>
      </div>

      <!-- 错误 -->
      <div v-else-if="error" class="cal-error">
        <div>⚠️ 加载直播记录失败</div>
        <div style="margin-top:.4rem;font-size:.8rem;color:var(--dim)">{{ error }}</div>
        <button class="retry-btn" @click="initCalendar">重新加载</button>
      </div>

      <!-- 日历主体 -->
      <div v-else>
        <div class="cal-weekdays">
          <span v-for="d in ['日','一','二','三','四','五','六']" :key="d">{{ d }}</span>
        </div>
        <div class="cal-grid">
          <div
            v-for="cell in cells"
            :key="cell.key"
            class="cal-day"
            :class="{
              'other-month': cell.otherMonth,
              'today': cell.isToday,
              'has-live': cell.hasLive,
              'selected': cell.key === selectedDate
            }"
            :title="cell.title"
            @click="cell.hasLive && selectDate(cell.key)"
          >
            <span>{{ cell.day }}</span>
            <div v-if="cell.hasLive" class="live-dot">
              <span
                v-for="i in Math.min(cell.count, 3)"
                :key="i"
                :class="{ multi: cell.count > 1 }"
              ></span>
            </div>
          </div>
        </div>

        <!-- 详情面板 -->
        <div v-if="selectedDate && liveMap[selectedDate]" class="cal-detail">
          <div class="cal-detail-date">📅 {{ detailDateLabel }}</div>
          <div v-if="detailIsValidDay" style="margin-bottom:1rem;padding:.5rem;background:rgba(77,240,200,.1);border:1px solid rgba(77,240,200,.3);border-radius:8px;color:var(--teal2);font-size:.85rem;font-weight:600;text-align:center">
            ✅ 今日有效直播时长 {{ (detailValidMins/60).toFixed(1) }}h (已计入有效天)
          </div>
          <div v-else style="margin-bottom:1rem;padding:.5rem;background:rgba(255,255,255,.05);border-radius:8px;color:var(--dim);font-size:.85rem;text-align:center">
            今日累计有效时长 {{ (detailValidMins/60).toFixed(1) }}h (未满2小时)
          </div>
          <div v-for="r in liveMap[selectedDate]" :key="r.title + r.start" class="live-record">
            <div class="live-record-icon">{{ parseDuration(r.duration) >= 30 ? '✅' : '⚠️' }}</div>
            <div class="live-record-info">
              <h4>{{ r.title || '直播' }}</h4>
              <div class="live-record-meta">
                <span v-if="r.start">🕐 {{ r.start }}</span>
                <span v-if="r.duration">⏱ {{ fmtDuration(r.duration) }}</span>
                <span v-for="t in (r.tags||[])" :key="t" class="badge">{{ t }}</span>
              </div>
            </div>
            <a v-if="r.url" :href="r.url" target="_blank" rel="noopener" class="replay-btn">回放 ↗</a>
          </div>
        </div>

        <!-- 统计 -->
        <div class="cal-stats">
          <div class="stat-chip" style="border-color:var(--teal2);background:rgba(77,240,200,.05)">
            <div class="num" style="color:var(--teal2)">{{ stats.validDays }}</div>
            <div class="lbl">本月有效直播天</div>
          </div>
          <div class="stat-chip">
            <div class="num">{{ stats.monthCount }}</div>
            <div class="lbl">本月直播场次</div>
          </div>
          <div class="stat-chip">
            <div class="num">{{ stats.totalH }}</div>
            <div class="lbl">本月有效总时长 (h)</div>
          </div>
          <div class="stat-chip">
            <div class="num">{{ stats.totalAll }}</div>
            <div class="lbl">历史总场次</div>
          </div>
        </div>
      </div>
    </div>
  </section>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'

const MONTHS = ['一月','二月','三月','四月','五月','六月','七月','八月','九月','十月','十一月','十二月']
const WEEKDAYS = ['周日','周一','周二','周三','周四','周五','周六']

const today = new Date()
const todayKey = toKey(today.getFullYear(), today.getMonth(), today.getDate())

const curYear  = ref(today.getFullYear())
const curMonth = ref(today.getMonth())
const selectedDate = ref(null)
const liveMap = ref({})
const loading = ref(true)
const error   = ref(null)

function toKey(y, m, d) {
  const dt = new Date(y, m, d)
  return `${dt.getFullYear()}-${String(dt.getMonth()+1).padStart(2,'0')}-${String(dt.getDate()).padStart(2,'0')}`
}

function parseDuration(raw) {
  if (!raw) return null
  raw = String(raw).trim().toLowerCase()
  let m = 0
  const hm = raw.match(/(\d+\.?\d*)\s*h/);  if (hm) m += parseFloat(hm[1]) * 60
  const mm = raw.match(/(\d+\.?\d*)\s*m(?!s)/); if (mm) m += parseFloat(mm[1])
  const nm = raw.match(/^(\d+\.?\d*)$/);     if (nm) m = parseFloat(nm[1])
  return m > 0 ? Math.round(m) : null
}

function fmtDuration(raw) {
  const m = parseDuration(raw)
  if (!m) return ''
  const h = Math.floor(m/60), min = m%60
  return h ? (min ? `${h}h ${min}min` : `${h}h`) : `${min}min`
}

function processRecords(records) {
  const out = []
  records.forEach(r => {
    if (!r.date || !/^\d{4}-\d{2}-\d{2}$/.test(r.date)) return
    const dur = parseDuration(r.duration)
    if (!r.start || !dur) { out.push({...r}); return }
    const [sh, sm] = r.start.split(':').map(Number)
    const startMin = sh*60 + sm
    const endMin   = startMin + dur
    if (endMin <= 1440) {
      out.push({...r})
    } else {
      const day1 = 1440 - startMin
      if (day1 > 0) out.push({...r, duration:`${day1}min`, title:`${r.title||'直播'} (前半段)`})
      const day2 = endMin - 1440
      if (day2 > 0) {
        const next = new Date(r.date); next.setDate(next.getDate()+1)
        out.push({...r, date: next.toISOString().slice(0,10), start:'00:00', duration:`${day2}min`, title:`${r.title||'直播'} (后半段)`})
      }
    }
  })
  return out
}

// 日历格子
const cells = computed(() => {
  const first = new Date(curYear.value, curMonth.value, 1).getDay()
  const daysInMonth = new Date(curYear.value, curMonth.value+1, 0).getDate()
  const daysInPrev  = new Date(curYear.value, curMonth.value, 0).getDate()
  const list = []

  for (let i = 0; i < first; i++) {
    const d = daysInPrev - first + 1 + i
    const k = toKey(curYear.value, curMonth.value-1, d)
    const recs = liveMap.value[k] || []
    list.push({ key:k, day:d, otherMonth:true, isToday:k===todayKey, hasLive:recs.length>0, count:recs.length, title:recs.map(r=>r.title).join(' / ') })
  }
  for (let d = 1; d <= daysInMonth; d++) {
    const k = toKey(curYear.value, curMonth.value, d)
    const recs = liveMap.value[k] || []
    list.push({ key:k, day:d, otherMonth:false, isToday:k===todayKey, hasLive:recs.length>0, count:recs.length, title:recs.map(r=>r.title).join(' / ') })
  }
  const total = first + daysInMonth
  const remain = total%7 === 0 ? 0 : 7 - (total%7)
  for (let d = 1; d <= remain; d++) {
    const k = toKey(curYear.value, curMonth.value+1, d)
    const recs = liveMap.value[k] || []
    list.push({ key:k, day:d, otherMonth:true, isToday:k===todayKey, hasLive:recs.length>0, count:recs.length, title:recs.map(r=>r.title).join(' / ') })
  }
  return list
})

// 详情面板计算
const detailValidMins = computed(() => {
  if (!selectedDate.value) return 0
  return (liveMap.value[selectedDate.value] || []).reduce((s,r) => {
    const d = parseDuration(r.duration); return s + (d >= 30 ? d : 0)
  }, 0)
})
const detailIsValidDay = computed(() => detailValidMins.value >= 120)
const detailDateLabel = computed(() => {
  if (!selectedDate.value) return ''
  const [y,m,d] = selectedDate.value.split('-').map(Number)
  return `${y}年${m}月${d}日 · ${WEEKDAYS[new Date(y,m-1,d).getDay()]}`
})

// 统计
const stats = computed(() => {
  const prefix = `${curYear.value}-${String(curMonth.value+1).padStart(2,'0')}`
  const monthKeys = Object.keys(liveMap.value).filter(k => k.startsWith(prefix))
  let validDays = 0, totalValidMins = 0
  monthKeys.forEach(k => {
    let dayMins = 0
    liveMap.value[k].forEach(r => { const d = parseDuration(r.duration); if (d>=30) { dayMins+=d; totalValidMins+=d } })
    if (dayMins >= 120) validDays++
  })
  const totalAll = Object.values(liveMap.value).reduce((s,v)=>s+v.length, 0)
  return {
    validDays,
    monthCount: monthKeys.reduce((s,k)=>s+liveMap.value[k].length, 0),
    totalH: (totalValidMins/60).toFixed(1),
    totalAll
  }
})

function selectDate(key) {
  selectedDate.value = selectedDate.value === key ? null : key
}
function prevMonth() {
  curMonth.value--; if (curMonth.value < 0) { curMonth.value=11; curYear.value-- }
  selectedDate.value = null
}
function nextMonth() {
  curMonth.value++; if (curMonth.value > 11) { curMonth.value=0; curYear.value++ }
  selectedDate.value = null
}
function goToday() {
  curYear.value = today.getFullYear(); curMonth.value = today.getMonth()
  selectedDate.value = todayKey
}

async function initCalendar() {
  loading.value = true; error.value = null
  try {
    const res = await fetch('/assets/data/live_history.json', { cache:'no-cache' })
    if (!res.ok) throw new Error(`HTTP ${res.status}`)
    const raw = await res.json()
    if (!Array.isArray(raw) || !raw.length) throw new Error('记录文件为空')
    const records = processRecords(raw)
    const map = {}
    records.forEach(r => {
      if (!r.date || !/^\d{4}-\d{2}-\d{2}$/.test(r.date)) return
      if (!map[r.date]) map[r.date] = []
      map[r.date].push(r)
    })
    liveMap.value = map
    if (map[todayKey]) selectedDate.value = todayKey
  } catch(e) {
    error.value = e.message
  } finally {
    loading.value = false
  }
}

onMounted(initCalendar)
</script>

<style scoped>
.cal-wrap {
  background: rgba(255,255,255,.02);
  border: 1px solid rgba(0,200,255,.12);
  border-radius: 24px;
  padding: 2.5rem 3rem;
  backdrop-filter: blur(12px);
}

.cal-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 1.6rem;
  flex-wrap: wrap;
  gap: 1rem;
}

.cal-title {
  font-size: 1.2rem;
  font-weight: 700;
  letter-spacing: .04em;
  background: linear-gradient(120deg,#fff,var(--teal));
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.cal-nav-group { display: flex; gap: .5rem; align-items: center }

.cal-nav-btn {
  background: rgba(0,200,255,.08);
  border: 1px solid rgba(0,200,255,.2);
  color: var(--teal);
  border-radius: 10px;
  width: 36px; height: 36px;
  display: flex; align-items: center; justify-content: center;
  cursor: pointer;
  transition: background .2s;
  font-size: 1rem;
}
.cal-nav-btn:hover { background: rgba(0,200,255,.18) }

.today-btn {
  background: rgba(0,200,255,.08);
  border: 1px solid rgba(0,200,255,.2);
  color: var(--teal);
  border-radius: 10px;
  padding: 0 .8rem; height: 36px;
  display: flex; align-items: center;
  cursor: pointer; font-size: .8rem; font-weight: 600;
  transition: background .2s;
}
.today-btn:hover { background: rgba(0,200,255,.18) }

.cal-legend { display: flex; gap: 1.4rem; flex-wrap: wrap; align-items: center }
.legend-item { display: flex; align-items: center; gap: .45rem; font-size: .75rem; color: var(--dim) }
.legend-dot { width: 8px; height: 8px; border-radius: 50% }
.legend-note {
  font-size: .7rem; color: var(--dim);
  background: rgba(0,200,255,.05);
  padding: .2rem .6rem; border-radius: 6px;
  border: 1px dashed rgba(0,200,255,.2);
  display: flex; align-items: center; gap: .4rem;
}

.cal-weekdays {
  display: grid;
  grid-template-columns: repeat(7,1fr);
  margin-bottom: .6rem;
}
.cal-weekdays span {
  text-align: center; font-size: .75rem; font-weight: 600;
  color: rgba(255,255,255,.3); letter-spacing: .06em; padding: .4rem 0;
}

.cal-grid { display: grid; grid-template-columns: repeat(7,1fr); gap: 8px }

.cal-day {
  aspect-ratio: 1;
  border-radius: 10px;
  display: flex; flex-direction: column;
  align-items: center; justify-content: center;
  position: relative; font-size: 1.1rem;
  transition: background .15s;
  user-select: none;
  min-height: 64px;
}
.cal-day.other-month { color: rgba(255,255,255,.15) }
.cal-day.today {
  background: rgba(0,200,255,.1);
  border: 1px solid rgba(0,200,255,.35);
  color: var(--teal); font-weight: 700;
}
.cal-day.has-live { cursor: pointer }
.cal-day.has-live:hover { background: rgba(0,200,255,.12) }
.cal-day.selected {
  background: rgba(0,200,255,.2) !important;
  border: 1px solid rgba(0,200,255,.5) !important;
}

.live-dot {
  position: absolute; bottom: 5px;
  left: 50%; transform: translateX(-50%);
  display: flex; gap: 3px;
}
.live-dot span {
  width: 5px; height: 5px; border-radius: 50%;
  background: var(--teal); box-shadow: 0 0 6px var(--teal);
}
.live-dot span.multi { background: var(--teal2) }

.cal-detail {
  margin-top: 1.4rem;
  background: rgba(0,200,255,.04);
  border: 1px solid rgba(0,200,255,.15);
  border-radius: 16px; padding: 1.4rem 1.6rem;
  animation: fadeIn .2s ease;
}
@keyframes fadeIn { from{opacity:0;transform:translateY(6px)} to{opacity:1;transform:translateY(0)} }

.cal-detail-date {
  font-size: .8rem; color: var(--teal);
  letter-spacing: .1em; margin-bottom: .8rem; font-weight: 600;
}

.live-record {
  display: flex; align-items: flex-start; gap: 1rem;
  padding: .8rem;
  background: rgba(255,255,255,.03);
  border-radius: 12px;
  border: 1px solid rgba(0,200,255,.08);
  margin-bottom: .6rem;
  transition: border-color .2s;
}
.live-record:last-child { margin-bottom: 0 }
.live-record:hover { border-color: rgba(0,200,255,.25) }
.live-record-icon {
  width: 36px; height: 36px; border-radius: 10px; flex-shrink: 0;
  background: linear-gradient(135deg,rgba(0,144,212,.4),rgba(0,200,255,.2));
  border: 1px solid rgba(0,200,255,.2);
  display: flex; align-items: center; justify-content: center; font-size: 1rem;
}
.live-record-info { flex: 1; min-width: 0 }
.live-record-info h4 { font-size: .9rem; margin-bottom: .3rem }
.live-record-meta { display: flex; gap: .8rem; flex-wrap: wrap }
.live-record-meta span { font-size: .75rem; color: var(--dim) }
.live-record-meta .badge {
  padding: .15rem .55rem; border-radius: 999px;
  background: rgba(0,200,255,.1); color: var(--teal);
  border: 1px solid rgba(0,200,255,.2);
}
.replay-btn {
  flex-shrink: 0; padding: .3rem .7rem; border-radius: 999px;
  background: rgba(0,200,255,.1); border: 1px solid rgba(0,200,255,.2);
  color: var(--teal); font-size: .75rem; text-decoration: none;
  white-space: nowrap; transition: background .2s;
}
.replay-btn:hover { background: rgba(0,200,255,.22) }

.cal-stats { display: flex; gap: 1.2rem; margin-top: 1.4rem; flex-wrap: wrap }
.stat-chip {
  flex: 1; min-width: 100px;
  background: rgba(255,255,255,.03);
  border: 1px solid rgba(0,200,255,.1);
  border-radius: 14px; padding: .9rem 1.2rem; text-align: center;
}
.stat-chip .num { font-size: 1.5rem; font-weight: 800; color: var(--teal); line-height: 1 }
.stat-chip .lbl { font-size: .75rem; color: var(--dim); margin-top: .3rem }

.cal-loading {
  text-align: center; padding: 3rem 1rem; color: var(--dim);
  display: flex; flex-direction: column; align-items: center; gap: 1rem;
}
.spinner {
  width: 36px; height: 36px; border-radius: 50%;
  border: 3px solid rgba(0,200,255,.15);
  border-top-color: var(--teal);
  animation: spin .8s linear infinite;
}
@keyframes spin { to { transform: rotate(360deg) } }

.cal-error {
  text-align: center; padding: 2rem;
  color: rgba(255,150,150,.8);
  background: rgba(255,80,80,.05);
  border: 1px solid rgba(255,80,80,.15);
  border-radius: 14px; font-size: .9rem;
}
.retry-btn {
  margin-top: .8rem; padding: .45rem 1.2rem; border-radius: 999px;
  background: rgba(255,100,100,.12); border: 1px solid rgba(255,100,100,.25);
  color: rgba(255,150,150,.9); cursor: pointer; font-size: .82rem;
  transition: background .2s;
}
.retry-btn:hover { background: rgba(255,100,100,.22) }

@media (max-width: 640px) {
  .cal-day { font-size: .78rem }
  .cal-stats { gap: .7rem }
  .cal-header { flex-direction: column; align-items: flex-start }
  .cal-legend { justify-content: flex-start; width: 100% }
}
</style>