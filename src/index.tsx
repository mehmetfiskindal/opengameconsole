import { ReactiveComponent, mount } from '@geastack/core'
import './styles.css'

export class App extends ReactiveComponent {
  template() {
    return (
      <div class="app">
        <div class="panel">
          <span class="eyebrow">GEASTACK</span>
          <span class="title">Opengameconsole</span>
          <span class="copy">One TypeScript app, ready for simulator and native targets.</span>
        </div>
      </div>
    )
  }
}

mount(App)
