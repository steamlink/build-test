document.onload = function() {
    JSONTest();
}

JSONTest = function() {
    var resultDiv = $("#resultDivContainer");
    $.ajax({
        url: "/api/v0/meshes",
        type: "GET",
        success: function (result) {
            console.log(result)
            switch (result) {
        },
        error: function (xhr, ajaxOptions, thrownError) {
            alert(xhr.status);
            alert(thrownError);
        }
    });
};
