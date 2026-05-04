<template>
  <section class="section" id="playlist">
    <p class="sec-tag">Music</p>
    <h2 class="sec-title">鲨宝歌单</h2>

    <div style="margin:1.5rem 0;display:flex;gap:.5rem;">
      <input
        v-model="query"
        type="text"
        placeholder="搜索歌曲、歌手..."
        class="search-input"
        @keyup.enter="() => {}"
      >
      <button class="search-btn" @click="() => {}">搜索</button>
    </div>

    <div class="playlist-container">
      <div
        v-for="(song, index) in filtered"
        :key="index"
        class="song-item"
        @click="song.url ? openUrl(song.url) : null"
        :title="song.url ? `点击跳转至：${song.title} (${song.artist})` : ''"
      >
        <div class="song-left">
          <div class="song-icon-box">{{ song.icon }}</div>
          <div class="song-info">
            <div class="song-title">{{ song.title }}</div>
            <div class="song-meta">
              <span>🎤 {{ song.artist }}</span>
              <span v-if="song.tags?.length"> · </span>
              <span v-for="t in song.tags" :key="t" class="song-tag">{{ t }}</span>
            </div>
          </div>
        </div>
        <div class="song-right">
          <span class="song-date">{{ song.date }}</span>
          <a
            v-if="song.url"
            :href="song.url"
            target="_blank"
            rel="noopener"
            class="song-action"
            @click.stop
          >
            <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
              <path d="M8 5v14l11-7z"/>
            </svg>
          </a>
          <div v-else class="song-action" style="opacity:.4;cursor:default;">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor">
              <path d="M8 5v14l11-7z"/>
            </svg>
          </div>
        </div>
      </div>

      <div v-if="filtered.length === 0" class="empty-tip">未找到相关歌曲 🎵</div>
    </div>
  </section>
</template>

<script setup>
import { ref, computed } from 'vue'

const query = ref('')

function openUrl(url) {
  window.open(url, '_blank')
}

const PLAYLIST = [
  { title: "给你一瓶魔法药水", artist: "告五人", date: "2025-12-31", tags: ["摇滚", "甜蜜", "太空"], icon: "🧪", url: "https://www.bilibili.com/video/BV13XiuBvE7b" },
  { title: "风", artist: "A-Lin", date: "2026-03-16", tags: ["抒情", "治愈"], icon: "🍃", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "认错", artist: "许嵩", date: "2026-03-16", tags: ["R&B", "经典"], icon: "🙇", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "幻听", artist: "许嵩", date: "2026-03-16", tags: ["中国风", "流行"], icon: "👂", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "阴天", artist: "莫文蔚", date: "2026-03-16", tags: ["爵士", "慵懒"], icon: "☁️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "素颜", artist: "许嵩/何曼婷", date: "2026-03-16", tags: ["对唱", "校园"], icon: "🎀", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "童话", artist: "光良", date: "2026-03-16", tags: ["钢琴", "经典"], icon: "🏰", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "传奇", artist: "李健", date: "2026-03-16", tags: ["空灵", "王菲翻唱"], icon: "✨", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "心墙", artist: "郭静", date: "2026-03-16", tags: ["清新", "林俊杰作曲"], icon: "🧱", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "你走", artist: "周延英", date: "2026-03-16", tags: ["伤感", "抖音热歌"], icon: "🚶", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "搁浅", artist: "周杰伦", date: "2026-03-16", tags: ["R&B", "悲伤"], icon: "⚓", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "画心", artist: "张靓颖", date: "2026-03-16", tags: ["影视", "海豚音"], icon: "🎨", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "宁夏", artist: "梁静茹", date: "2026-03-16", tags: ["夏天", "清新"], icon: "🍉", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "情花", artist: "邓丽君", date: "2026-03-16", tags: ["复古", "经典"], icon: "🌹", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "太阳", artist: "邱振哲", date: "2026-03-16", tags: ["励志", "温暖"], icon: "☀️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "着魔", artist: "张杰", date: "2026-03-16", tags: ["高音", "仙侠"], icon: "🔥", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "体面", artist: "于文文", date: "2026-03-16", tags: ["分手", "电影"], icon: "💔", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "依恋", artist: "蔡依林", date: "2026-03-16", tags: ["舞曲", "深情"], icon: "🔗", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "残雪", artist: "许嵩", date: "2026-03-16", tags: ["古风", "早期"], icon: "❄️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "夏天", artist: "李玖哲", date: "2026-03-16", tags: ["轻快", "R&B"], icon: "🏖️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "大鱼", artist: "周深", date: "2026-03-16", tags: ["空灵", "国风"], icon: "🐟", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "青花", artist: "周传雄", date: "2026-03-16", tags: ["中国风", "经典"], icon: "🏺", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "花海", artist: "周杰伦", date: "2026-03-16", tags: ["浪漫", "岛歌"], icon: "🌸", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "彩虹", artist: "周杰伦", date: "2026-03-16", tags: ["温柔", "吉他"], icon: "🌈", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "安静", artist: "周杰伦", date: "2026-03-16", tags: ["钢琴", "失恋"], icon: "🤫", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "江南", artist: "林俊杰", date: "2026-03-16", tags: ["中国风", "经典"], icon: "🛶", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "晴天", artist: "周杰伦", date: "2026-03-16", tags: ["校园", "神曲"], icon: "⛅", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "轨迹", artist: "周杰伦", date: "2026-03-16", tags: ["电影", "伤感"], icon: "🛰️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱你", artist: "王心凌", date: "2026-03-16", tags: ["甜心", "舞曲"], icon: "🍬", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "倒带", artist: "蔡依林", date: "2026-03-16", tags: ["周杰伦作曲", "伤感"], icon: "⏪", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "王妃", artist: "萧敬腾", date: "2026-03-16", tags: ["摇滚", "爆发力"], icon: "👑", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "暖暖", artist: "梁静茹", date: "2026-03-16", tags: ["甜蜜", "婚礼"], icon: "🧣", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "泡沫", artist: "G.E.M.邓紫棋", date: "2026-03-16", tags: ["高音", "爆发"], icon: "🫧", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "心跳", artist: "王力宏", date: "2026-03-16", tags: ["R&B", "深情"], icon: "💓", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱错", artist: "王力宏", date: "2026-03-16", tags: ["摇滚", "遗憾"], icon: "❌", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "后来", artist: "刘若英", date: "2026-03-16", tags: ["经典", "催泪"], icon: "🕰️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "何须问", artist: "银临", date: "2026-03-16", tags: ["古风", "白蛇"], icon: "🐍", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "甜甜的", artist: "周杰伦", date: "2026-03-16", tags: ["甜蜜", "经典"], icon: "🍦", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "下雨天", artist: "南拳妈妈", date: "2026-03-16", tags: ["经典", "雨天"], icon: "☔", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "红颜旧", artist: "刘涛", date: "2026-03-16", tags: ["影视", "琅琊榜"], icon: "🏯", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "一个人", artist: "林俊杰", date: "2026-03-16", tags: ["孤独", "抒情"], icon: "🚶‍♂️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "安和桥", artist: "宋冬野", date: "2026-03-16", tags: ["民谣", "鼓点"], icon: "🌉", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "虫儿飞", artist: "郑伊健", date: "2026-03-16", tags: ["儿歌", "温馨"], icon: "🦋", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "喜欢你", artist: "Beyond", date: "2026-03-16", tags: ["粤语", "经典"], icon: "❤️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "芙蓉雨", artist: "刘珂矣", date: "2026-03-16", tags: ["禅意", "古风"], icon: "🌧️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "半壶纱", artist: "刘珂矣", date: "2026-03-16", tags: ["禅意", "国风"], icon: "🍵", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "醉赤壁", artist: "林俊杰", date: "2026-03-16", tags: ["三国", "中国风"], icon: "⚔️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "甜蜜蜜", artist: "邓丽君", date: "2026-03-16", tags: ["复古", "甜美"], icon: "🍯", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "发如雪", artist: "周杰伦", date: "2026-03-16", tags: ["中国风", "经典"], icon: "❄️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "七月上", artist: "Jam", date: "2026-03-16", tags: ["民谣", "江湖"], icon: "🌙", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "萱草花", artist: "张小斐", date: "2026-03-16", tags: ["电影", "温情"], icon: "🌼", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "日不落", artist: "蔡依林", date: "2026-03-16", tags: ["舞曲", "经典"], icon: "💃", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "怎么啦", artist: "周兴哲", date: "2026-03-16", tags: ["深情", "追问"], icon: "❓", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "美人鱼", artist: "林俊杰", date: "2026-03-16", tags: ["童话", "海洋"], icon: "🧜‍♀️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "简单爱", artist: "周杰伦", date: "2026-03-16", tags: ["初恋", "轻松"], icon: "🚲", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "七里香", artist: "周杰伦", date: "2026-03-16", tags: ["夏天", "诗意的"], icon: "📖", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱啦啦", artist: "海楠", date: "2026-03-16", tags: ["可爱", "早期网络"], icon: "🎀", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "多幸运", artist: "韩安旭", date: "2026-03-16", tags: ["甜蜜", "抖音"], icon: "🍀", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "十一年", artist: "邱永传", date: "2026-03-16", tags: ["伤感", "时间"], icon: "🗓️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "黑色幽默", artist: "周杰伦", date: "2026-03-16", tags: ["R&B", "经典"], icon: "🎹", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "回到过去", artist: "周杰伦", date: "2026-03-16", tags: ["怀旧", "R&B"], icon: "🔙", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "最美的光", artist: "孙霄磊", date: "2026-03-16", tags: ["正能量", "光"], icon: "💡", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "断了的弦", artist: "周杰伦", date: "2026-03-16", tags: ["中国风", "遗憾"], icon: "🎻", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "一路生花", artist: "温奕心", date: "2026-03-16", tags: ["励志", "祝福"], icon: "🌸", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "开始懂了", artist: "孙燕姿", date: "2026-03-16", tags: ["成长", "经典"], icon: "📘", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "不是故意", artist: "王祖蓝", date: "2026-03-16", tags: ["搞怪", "轻松"], icon: "🤷", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "一路向北", artist: "周杰伦", date: "2026-03-16", tags: ["电影", "悲伤"], icon: "🧭", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱的魔法", artist: "金莎", date: "2026-03-16", tags: ["甜蜜", "魔法"], icon: "🪄", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "美丽女人", artist: "刘嘉亮", date: "2026-03-16", tags: ["赞美", "流行"], icon: "💃", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "风吹麦浪", artist: "李健", date: "2026-03-16", tags: ["田园", "唯美"], icon: "🌾", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "有何不可", artist: "许嵩", date: "2026-03-16", tags: ["甜蜜", "轻快"], icon: "🆗", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "纸短情长", artist: "烟把儿乐队", date: "2026-03-16", tags: ["民谣", "叙事"], icon: "✉️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "黑色毛衣", artist: "周杰伦", date: "2026-03-16", tags: ["R&B", "忧郁"], icon: "🧥", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱丫爱丫", artist: "BY2", date: "2026-03-16", tags: ["甜美", "经典"], icon: "👯", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "星语星愿", artist: "张柏芝", date: "2026-03-16", tags: ["电影", "经典"], icon: "⭐", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "星月神话", artist: "金莎", date: "2026-03-16", tags: ["穿越", "神话"], icon: "🌙", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "断桥残雪", artist: "许嵩", date: "2026-03-16", tags: ["中国风", "西湖"], icon: "🌨️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "烟花易冷", artist: "周杰伦", date: "2026-03-16", tags: ["中国风", "凄美"], icon: "🎆", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "白色风车", artist: "周杰伦", date: "2026-03-16", tags: ["异域", "爱情"], icon: "🎡", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "你的答案", artist: "阿冗", date: "2026-03-16", tags: ["励志", "爆发"], icon: "💡", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "千里之外", artist: "周杰伦/费玉清", date: "2026-03-16", tags: ["对唱", "中国风"], icon: "📨", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "最长的电影", artist: "周杰伦", date: "2026-03-16", tags: ["钢琴", "悲伤"], icon: "🎬", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "下一个天亮", artist: "郭静", date: "2026-03-16", tags: ["治愈", "等待"], icon: "🌅", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "靠近一点点", artist: "南拳妈妈", date: "2026-03-16", tags: ["甜蜜", "暧昧"], icon: "🤏", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "新贵妃醉酒", artist: "李玉刚", date: "2026-03-16", tags: ["戏腔", "经典"], icon: "🎭", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "可惜没如果", artist: "林俊杰", date: "2026-03-16", tags: ["遗憾", "钢琴"], icon: "😢", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "我只在乎你", artist: "邓丽君", date: "2026-03-16", tags: ["经典", "深情"], icon: "🤝", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "我的歌声里", artist: "曲婉婷", date: "2026-03-16", tags: ["原创", "怀念"], icon: "🎤", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "山楂树之恋", artist: "程佳佳", date: "2026-03-16", tags: ["民谣", "纯真"], icon: "🌳", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "其实我介意", artist: "小雪/汉洋", date: "2026-03-16", tags: ["粤语", "对唱"], icon: "😶", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "我会好好的", artist: "伍佰", date: "2026-03-16", tags: ["安慰", "摇滚"], icon: "👋", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "我们的纪念", artist: "Palaye Royale", date: "2026-03-16", tags: ["英文", "摇滚"], icon: "🎸", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "可惜不是你", artist: "梁静茹", date: "2026-03-16", tags: ["遗憾", "经典"], icon: "🥀", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "我们都一样", artist: "张杰", date: "2026-03-16", tags: ["励志", "粉丝"], icon: "🤜", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "玫瑰花的葬礼", artist: "许嵩", date: "2026-03-16", tags: ["早期", "非主流"], icon: "⚰️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "有可能的夜晚", artist: "曾轶可", date: "2026-03-16", tags: ["迷幻", "独特"], icon: "🌃", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "爱的飞行日记", artist: "周杰伦/杨瑞代", date: "2026-03-16", tags: ["太空", "对唱"], icon: "✈️", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "阿拉斯加海湾", artist: "蓝心羽", date: "2026-03-16", tags: ["温柔", "治愈"], icon: "🌊", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "不能说的秘密", artist: "周杰伦", date: "2026-03-16", tags: ["电影", "钢琴"], icon: "🤐", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "乌兰巴托的夜", artist: "谭维维", date: "2026-03-16", tags: ["民歌", "辽阔"], icon: "🐎", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "贝多芬的忧伤", artist: "李乐祺", date: "2026-03-16", tags: ["古典", "伤感"], icon: "🎹", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "世界赠予我的", artist: "王菲", date: "2026-03-16", tags: ["春晚", "哲理"], icon: "🎁", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "烟火里的尘埃", artist: "华晨宇", date: "2026-03-16", tags: ["高音", "独特"], icon: "🎇", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "父亲写的散文诗", artist: "许飞", date: "2026-03-16", tags: ["亲情", "催泪"], icon: "📝", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "全世界宣布爱你", artist: "孙子涵", date: "2026-03-16", tags: ["甜蜜", "对唱"], icon: "🌍", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "亲爱的还幸福吗", artist: "金莎", date: "2026-03-16", tags: ["问候", "伤感"], icon: "💌", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "第57次取消发送", artist: "任然", date: "2026-03-16", tags: ["纠结", "暗恋"], icon: "📵", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "这世界有那么多人", artist: "莫文蔚", date: "2026-03-16", tags: ["电影", "缘分"], icon: "👫", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "Melody", artist: "陶喆", date: "2026-03-16", tags: ["R&B", "经典"], icon: "🎼", url: "https://www.bilibili.com/video/BV1GJ411x7h7" },
  { title: "Super Star", artist: "S.H.E", date: "2026-03-16", tags: ["摇滚", "经典"], icon: "🌟", url: "https://www.bilibili.com/video/BV1GJ411x7h7" }
]

const filtered = computed(() => {
  const q = query.value.toLowerCase().trim()
  if (!q) return PLAYLIST
  return PLAYLIST.filter(s =>
    s.title.toLowerCase().includes(q) ||
    s.artist.toLowerCase().includes(q) ||
    s.tags.some(t => t.toLowerCase().includes(q))
  )
})
</script>

<style scoped>
.search-input {
  flex: 1;
  padding: .6rem 1rem;
  border-radius: 8px;
  border: 1px solid rgba(0,200,255,.2);
  background: rgba(255,255,255,.05);
  color: #fff;
  outline: none;
  font-size: .9rem;
  transition: border-color .2s;
}
.search-input:focus { border-color: rgba(0,200,255,.5) }
.search-input::placeholder { color: rgba(255,255,255,.3) }

.search-btn {
  padding: .6rem 1.2rem;
  border-radius: 8px;
  border: none;
  background: var(--teal);
  color: #fff;
  cursor: pointer;
  font-weight: 600;
  font-size: .9rem;
  transition: opacity .2s;
}
.search-btn:hover { opacity: .85 }

.playlist-container {
  display: flex;
  flex-direction: column;
  gap: .8rem;
  margin-top: 1rem;
}

.song-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: rgba(255,255,255,.03);
  border: 1px solid rgba(0,200,255,.08);
  border-radius: 12px;
  padding: 1rem 1.2rem;
  transition: all .2s ease;
  cursor: pointer;
  position: relative;
  overflow: hidden;
}
.song-item:hover {
  background: rgba(0,200,255,.08);
  border-color: rgba(0,200,255,.4);
  transform: translateX(5px);
}
.song-item::before {
  content: '';
  position: absolute;
  left: 0; top: 0; bottom: 0;
  width: 4px;
  background: linear-gradient(to bottom, var(--teal), var(--teal2));
  opacity: 0;
  transition: opacity .2s;
}
.song-item:hover::before { opacity: 1 }

.song-left {
  display: flex;
  align-items: center;
  gap: 1rem;
  flex: 1;
  min-width: 0;
}

.song-icon-box {
  width: 50px; height: 50px;
  border-radius: 10px;
  background: linear-gradient(135deg,rgba(0,144,212,.3),rgba(0,200,255,.1));
  border: 1px solid rgba(0,200,255,.2);
  display: flex; align-items: center; justify-content: center;
  font-size: 1.5rem;
  flex-shrink: 0;
}

.song-info {
  display: flex;
  flex-direction: column;
  gap: .25rem;
  min-width: 0;
}

.song-title {
  font-size: 1rem;
  font-weight: 600;
  color: #fff;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.song-meta {
  font-size: .8rem;
  color: var(--dim);
  display: flex;
  align-items: center;
  gap: .4rem;
  flex-wrap: wrap;
}

.song-tag {
  font-size: .65rem;
  padding: .1rem .4rem;
  border-radius: 4px;
  background: rgba(255,126,179,.1);
  color: var(--pink);
  border: 1px solid rgba(255,126,179,.2);
}

.song-right {
  display: flex;
  align-items: center;
  gap: 1rem;
  flex-shrink: 0;
  margin-left: 1rem;
}

.song-date {
  font-size: .75rem;
  color: rgba(255,255,255,.3);
  white-space: nowrap;
}

.song-action {
  width: 32px; height: 32px;
  border-radius: 50%;
  background: rgba(0,200,255,.1);
  border: 1px solid rgba(0,200,255,.2);
  color: var(--teal);
  display: flex; align-items: center; justify-content: center;
  transition: all .2s;
  flex-shrink: 0;
  text-decoration: none;
}
.song-item:hover .song-action {
  background: var(--teal);
  color: #fff;
  border-color: var(--teal);
}

.empty-tip {
  text-align: center;
  color: var(--dim);
  padding: 2rem;
}

@media (max-width: 640px) {
  .song-item { padding: .8rem }
  .song-icon-box { width: 40px; height: 40px; font-size: 1.2rem }
  .song-date { display: none }
}
</style>