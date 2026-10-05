import { ReactiveComponent, mount } from '@geastack/core'
import './styles.css'

function playError(code: number): string {
  if (code === 1) return 'ROM açılamadı'
  if (code === 2) return 'NES başlamadı'
  return 'Bilinmeyen hata'
}

export class App extends ReactiveComponent {
  screen = 'shell'
  message = ''

  play() {
    const code = nesPlay()
    if (code === 0) {
      this.message = ''
      this.screen = 'game'
    } else {
      this.message = playError(code)
    }
  }

  back() {
    nesStop()
    this.screen = 'shell'
  }

  template() {
    return (
      <div class="app">
        <div class={this.screen === 'shell' ? 'shell' : 'hidden'}>
          <span class="title">Opengameconsole</span>
          <span class="game-name">Pad Demo</span>
          <button class="play" onClick={() => this.play()}>Oyna</button>
          <span class="message">{this.message}</span>
        </div>
        <div class={this.screen === 'game' ? 'game' : 'hidden'}>
          <div class="screen-band"></div>
          <div class="controls">
            <div class="top-row">
              <button class="back" onClick={() => this.back()}>Geri</button>
            </div>
            <div class="pad-row">
              <div class="dpad">
                <div class="key" onTouchStart={() => nesSetButton('up', 1)} onTouchEnd={() => nesSetButton('up', 0)}>
                  <span>↑</span>
                </div>
                <div class="dpad-middle">
                  <div class="key" onTouchStart={() => nesSetButton('left', 1)} onTouchEnd={() => nesSetButton('left', 0)}>
                    <span>←</span>
                  </div>
                  <div class="key-gap"></div>
                  <div class="key" onTouchStart={() => nesSetButton('right', 1)} onTouchEnd={() => nesSetButton('right', 0)}>
                    <span>→</span>
                  </div>
                </div>
                <div class="key" onTouchStart={() => nesSetButton('down', 1)} onTouchEnd={() => nesSetButton('down', 0)}>
                  <span>↓</span>
                </div>
              </div>
              <div class="face">
                <div class="face-key" onTouchStart={() => nesSetButton('b', 1)} onTouchEnd={() => nesSetButton('b', 0)}>
                  <span>B</span>
                </div>
                <div class="face-key" onTouchStart={() => nesSetButton('a', 1)} onTouchEnd={() => nesSetButton('a', 0)}>
                  <span>A</span>
                </div>
              </div>
            </div>
            <div class="menu-row">
              <div class="menu-key" onTouchStart={() => nesSetButton('start', 1)} onTouchEnd={() => nesSetButton('start', 0)}>
                <span>Start</span>
              </div>
              <div class="menu-key" onTouchStart={() => nesSetButton('select', 1)} onTouchEnd={() => nesSetButton('select', 0)}>
                <span>Select</span>
              </div>
            </div>
          </div>
        </div>
      </div>
    )
  }
}

mount(App)
