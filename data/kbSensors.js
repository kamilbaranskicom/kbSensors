init();

var address, friendlyName, compensation, temperature;
var openEditTr = false;

function init() {
  document.querySelectorAll("tr").forEach(function (tr) {
    tr.addEventListener("dblclick", () => {
      editSensor(tr);
    });
  });
}

function editSensor(tr) {
  // close if opened
  closeEditTr();

  openEditTr = tr;

  td1 = tr.querySelector("td:nth-child(1)");
  td2 = tr.querySelector("td:nth-child(2)");

  address = td1.title;
  friendlyName = td1.innerText;
  compensation = td2.title;
  temperature = td2.innerText;

  td1.innerHTML = 'name: <input name="friendlyName" type="text" value="' + friendlyName + '" />';
  td2.innerHTML = 'compensation: <input name="compensation" type="number" size=6 value="' + compensation + '" step="0.01" /><input type="submit" value="OK!" /><input type="button" id="cancel" value="Cancel" />';

  document.querySelector("#cancel").addEventListener("click", () => {
    closeEditTr();
  });

  form = document.querySelector("form");
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    sendNewSensorData();
  });

  // console.log(address, friendlyName, compensation, temperature);
}

function closeEditTr() {
  if (openEditTr) {
    td1 = openEditTr.querySelector("td:nth-child(1)");
    td2 = openEditTr.querySelector("td:nth-child(2)");

    // td1.address hasn't changed
    td1.innerHTML = friendlyName;
    td2.title = compensation;
    td2.innerHTML = temperature;

    openEditTr = false;
  }
}

function sendNewSensorData() {
  friendlyName = td1.querySelector("input").value;
  compensation = td2.querySelector("input").value;

  // maybe too restrictive
  friendlyName = friendlyName.replace(/[&\/\\#,+()$~%.'":*?<>{}]/g, "_");

  const url = "/edit?address=" + address + "&friendlyName=" + friendlyName + "&compensation=" + compensation;

  fetch(url)
    .then((response) => response.text())
    .then((data) => {
      console.log(data);
      window.location.reload(true);
    });
  closeEditTr();
}

function refreshPage(e) {
  e.preventDefault();
  document.documentElement.classList.add("cursor-wait");
  document.body.style.opacity = "50%";
  window.location.reload();
}

function validateWebPasswordForm() {
  const pass1 = document.forms[0]["webpass"].value;
  const pass2 = document.forms[0]["webpass2"].value;

  if (pass1 !== pass2) {
    alert("WWW passwords do not match!");
    return false;
  }
  return true;
}

const gear = document.getElementById("settingsIcon");
const menu = document.getElementById("settingsMenu");

gear.addEventListener("click", (e) => {
  e.stopPropagation(); // nie zamyka menu przy kliknięciu w ikonę
  menu.classList.toggle("show");
});

// ukrycie menu po kliknięciu poza nim
document.addEventListener("click", () => {
  menu.classList.remove("show");
});

function toggleRemoteInputs() {
  const check = (prefix) => {
    const method = document.getElementById(prefix + "Method").value;
    const srcInput = document.getElementById(prefix + "Source");
    const paramInput = document.getElementById(prefix + "Param");

    // 0 = Brak, 1 = MQTT, 2 = HTTP, 3 = Local
    if (method == "0") {
      srcInput.style.display = "none";
      paramInput.style.display = "none";
    } else if (method == "3") {
      // LOCAL
      srcInput.style.display = "none"; // Nie potrzebujemy URL/Tematu
      paramInput.style.display = "inline-block"; // Potrzebujemy adresu (np. DHTtemp)
      paramInput.placeholder = "Adres lokalny (np. DHTtemp)";
    } else {
      // MQTT lub HTTP
      srcInput.style.display = "inline-block";
      paramInput.style.display = "inline-block";
      paramInput.placeholder = method == "1" ? "Opcjonalny klucz JSON" : "Adres sensora";
    }
  };
  check("remTemp");
  check("remHumi");
}
