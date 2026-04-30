const tabs = [
  "Overview",
  "Inventory",
  "Levels",
  "Upgrades",
  "Jokers",
  "Rewards",
  "Other"
];

const state = {
  currentTab: 0,
  rows: [],
  selected: null,
  loaded: false,
  loading: false,
  canSave: false
};

const tabList = document.getElementById("tabList");
const rowList = document.getElementById("rowList");
const tabTitle = document.getElementById("tabTitle");
const rowMetric = document.getElementById("rowMetric");
const saveBadge = document.getElementById("saveBadge");
const summaryText = document.getElementById("summaryText");
const statusText = document.getElementById("statusText");
const selectedTitle = document.getElementById("selectedTitle");
const fieldType = document.getElementById("fieldType");
const detailText = document.getElementById("detailText");
const valueInput = document.getElementById("valueInput");
const applyButton = document.getElementById("applyButton");
const loadingSpinner = document.getElementById("loadingSpinner");

function post(message) {
  window.chrome?.webview?.postMessage(message);
}

function encodeValue(value) {
  return encodeURIComponent(value ?? "");
}

function renderTabs() {
  tabList.innerHTML = "";
  tabList.style.setProperty("--total-radio", String(tabs.length));
  tabList.style.setProperty("--active-index", String(state.currentTab));
  tabs.forEach((label, index) => {
    const input = document.createElement("input");
    const slug = label.toLowerCase().replace(/[^a-z0-9]+/g, "-");
    input.type = "radio";
    input.name = "sidebar-tab";
    input.id = `radio-${slug}`;
    input.checked = state.currentTab === index;
    input.addEventListener("change", () => {
      if (input.checked) {
        post(`tab:${index}`);
      }
    });

    const tabLabel = document.createElement("label");
    tabLabel.htmlFor = input.id;
    tabLabel.textContent = label;

    tabList.appendChild(input);
    tabList.appendChild(tabLabel);
  });

  const gliderContainer = document.createElement("div");
  gliderContainer.className = "glider-container";
  gliderContainer.innerHTML = '<div class="glider"></div>';
  tabList.appendChild(gliderContainer);
}

function renderRows() {
  rowList.innerHTML = "";
  if (!state.rows.length) {
    const empty = document.createElement("div");
    empty.className = "row-card";
    empty.innerHTML = `<span class="row-title">No fields available</span><span class="row-type">Load a save or switch tabs</span>`;
    rowList.appendChild(empty);
    return;
  }

  state.rows.forEach((row) => {
    const card = document.createElement("button");
    card.type = "button";
    card.className = `row-card${state.selected?.id === row.id ? " active" : ""}`;
    card.innerHTML = `
      <span class="row-title">${escapeHtml(row.label)}</span>
      <span class="row-inline-meta">
        <span class="row-value">${escapeHtml(row.value)}</span>
        <span class="row-type">${escapeHtml(row.type)}</span>
      </span>
    `;
    card.addEventListener("click", () => post(`select:${encodeValue(row.id)}`));
    rowList.appendChild(card);
  });
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#39;");
}

function renderSelected() {
  if (!state.selected) {
    selectedTitle.textContent = "No field selected";
    fieldType.textContent = "Type";
    detailText.textContent = "Select a field from the current tab to inspect or edit it.";
    valueInput.value = "";
    valueInput.disabled = true;
    applyButton.disabled = true;
    return;
  }

  selectedTitle.textContent = state.selected.label;
  fieldType.textContent = state.selected.type;
  detailText.textContent = state.selected.detail;
  valueInput.value = state.selected.value ?? "";
  valueInput.disabled = !state.selected.editable || state.loading;
  applyButton.disabled = !state.selected.editable || state.loading;
}

function renderButtons() {
  document.querySelectorAll("[data-command='save'], [data-command='saveAs'], [data-command^='action:']").forEach((button) => {
    button.disabled = !state.canSave;
  });
  document.querySelectorAll("[data-command='open'], [data-command='autoImport'], [data-command='openFolder']").forEach((button) => {
    button.disabled = state.loading;
  });
}

function renderAll() {
  tabTitle.textContent = state.currentTabTitle || tabs[state.currentTab];
  rowMetric.textContent = `${state.rowsVisible ?? 0} shown`;
  saveBadge.textContent = state.loaded ? state.saveName : "No save loaded";
  summaryText.textContent = state.summary || "Open a save or use auto import to begin.";
  statusText.textContent = state.status || "";
  document.body.classList.toggle("is-loading", Boolean(state.loading));
  if (loadingSpinner) {
    loadingSpinner.hidden = !state.loading;
  }
  renderTabs();
  renderRows();
  renderSelected();
  renderButtons();
}

window.chrome?.webview?.addEventListener("message", (event) => {
  Object.assign(state, event.data || {});
  renderAll();
});

document.querySelectorAll("[data-command]").forEach((button) => {
  button.addEventListener("click", () => post(button.dataset.command));
});

applyButton.addEventListener("click", () => {
  post(`apply:${encodeValue(valueInput.value)}`);
});

renderTabs();
renderSelected();
post("ready");
