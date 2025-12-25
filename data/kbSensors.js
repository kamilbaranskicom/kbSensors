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

  td1.innerHTML =
    'name: <input name="friendlyName" type="text" value="' +
    friendlyName +
    '" />';
  td2.innerHTML =
    'compensation: <input name="compensation" type="number" size=6 value="' +
    compensation +
    '" step="0.01" /><input type="submit" value="OK!" /><input type="button" id="cancel" value="Cancel" />';

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

  const url =
    "/edit?address=" +
    address +
    "&friendlyName=" +
    friendlyName +
    "&compensation=" +
    compensation;

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
